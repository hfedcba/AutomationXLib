// This is the main DLL file.

#include "stdafx.h"

#include "AX.h"

namespace AutomationX
{
	bool AX::Connected::get()
	{ 
		return AxIsRunning() && AxIsHostMaster();
	}

	AX::AX()
	{
		AxInit(); //First function to call
		//AxOmAttachToObjectMemory must be called after AxInit
		if (!AxOmAttachToObjectMemory()) throw gcnew AXException("Could not attach to shared memory. Make sure AutomationX is running and the user running this program has enough privileges.");
		AxOmQueryProcessGroupInfo(); //No interpretable return value, must be called after AxOmAttachToObjectMemory
		_spsId = new int;
		AxHasSpsIdChanged(_spsId);
		_workerTimer = gcnew Timers::Timer(1000);
		_workerTimer->Elapsed += gcnew System::Timers::ElapsedEventHandler(this, &AutomationX::AX::OnWorkerTimerElapsed);
		_workerTimer->Start();
	}

	AX::~AX()
	{
		while (_spsIdChangedThread->ThreadState == ThreadState::Unstarted) Thread::Sleep(10);
		while (_spsIdChangedThread && (_spsIdChangedThread->ThreadState == ThreadState::Running || _spsIdChangedThread->ThreadState == ThreadState::WaitSleepJoin)) _spsIdChangedThread->Join();
		if (_spsId)	delete _spsId;
	}

	void AX::OnWorkerTimerElapsed(System::Object ^sender, System::Timers::ElapsedEventArgs ^e)
	{
		if (AxShutdownEvent() == 1 || !Connected) ShuttingDown(this);
	}

	void AX::CheckRunning()
	{
		if (!Connected) throw gcnew AXNotRunningException("aX is not running.");
	}

	void AX::Shutdown()
	{
		ShuttingDown(this);
		AxSendShutdownEvent();
	}

	System::Collections::Generic::List<String^>^ AX::GetInstanceNames(String^ className)
	{
		System::Collections::Generic::List<String^>^ instanceNames = gcnew System::Collections::Generic::List<String^>();
		char* cName = _converter.GetCString(className);
		AX_INSTANCE data = nullptr;
		while (data = AxGetInstance(data, cName))
		{
			String^ instanceName = gcnew String(AxGetInstanceName(data));
			if (instanceName->Length == 0) continue;
			instanceNames->Add(instanceName);
		}
		Marshal::FreeHGlobal(IntPtr((void*)cName)); //Always free memory!
		return instanceNames;
	}

	System::Collections::Generic::List<String^>^ AX::GetClassNames()
	{
		System::Collections::Generic::List<String^>^ classNames = gcnew System::Collections::Generic::List<String^>();
		std::vector<char*> buffers(1);
		AxGetAllClasses(&buffers.at(0), 1);
		int numberOfClasses = AxGetNumberOfAllClasses();
		buffers.resize(numberOfClasses);
		int count = AxGetAllClasses(&buffers.at(0), numberOfClasses);
		for (int i = 0; i < count; i++)
		{
			String^ name = gcnew String(buffers.at(i));
			if (name->Length == 0) continue;
			classNames->Add(name);
		}
		return classNames;
	}

	System::String^ AX::GetClassPath(String^ instanceName)
	{
		char* cName = _converter.GetCString(instanceName);
		void* handle = AxQueryInstance(cName);
		Marshal::FreeHGlobal(IntPtr((void*)cName)); //Always free memory!
		if (!handle) throw (AXException^)(gcnew AXInstanceException("Could not get instance handle for " + instanceName + "."));
		String^ value = gcnew String(AxGetInstanceClassPath(handle));
		return value;
	}

	bool AX::CheckSpsId()
	{
		if (AxHasSpsIdChanged(_spsId) == 1)
		{
			_lastSpsIdChange = DateTime::Now;
			try
			{
				_spsIdChangedThreadMutex.WaitOne();
				while (_spsIdChangedThread && _spsIdChangedThread->ThreadState == ThreadState::Unstarted) Thread::Sleep(10);
				while (_spsIdChangedThread && (_spsIdChangedThread->ThreadState == ThreadState::Running || _spsIdChangedThread->ThreadState == ThreadState::WaitSleepJoin)) _spsIdChangedThread->Join();

				_spsIdChangedThread = gcnew Thread(gcnew ThreadStart(this, &AX::RaiseSpsIdChanged));
				_spsIdChangedThread->Priority = ThreadPriority::Highest;
				_spsIdChangedThread->Start();
				_spsIdChangedThreadMutex.ReleaseMutex();
			}
			catch (Exception^ ex)
			{
				_spsIdChangedThreadMutex.ReleaseMutex();
				throw ex;
			}
			return true;
		}
		if(DateTime::Now.Subtract(_lastSpsIdChange).TotalMilliseconds > 10) return false;
		return true;
	}

	void AX::RaiseSpsIdChanged()
	{
		SpsIdChanged(this);
	}

	void AX::WriteJournal(int priority, String^ position, String^ message, String^ value, String^ fileName)
	{
		WriteJournal(priority, position, message, value, fileName, DateTime::Now);
	}

	void AX::WriteJournal(int priority, String^ position, String^ message, String^ value, String^ fileName, DateTime time)
	{
		Double aXTime = AxConvertDataTime_ToValue(time.Year, time.Month, time.Day, time.Hour, time.Minute, time.Second, time.Millisecond);
		char* pPosition = _converter.GetCString(position);
		char* pMessage = _converter.GetCString(message);
		char* pValue = _converter.GetCString(value);
		char* pFileName = _converter.GetCString(fileName);
		AxLogTS(priority, pPosition, pMessage, pValue, pFileName, aXTime);
		Marshal::FreeHGlobal(IntPtr((void*)pPosition));
		Marshal::FreeHGlobal(IntPtr((void*)pMessage));
		Marshal::FreeHGlobal(IntPtr((void*)pValue));
		Marshal::FreeHGlobal(IntPtr((void*)pFileName));
	}
}
