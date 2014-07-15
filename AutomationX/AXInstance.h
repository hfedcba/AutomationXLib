#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Collections::ObjectModel;
using namespace System::Threading;

#include "ManagedTypeConverter.h"
#include "AXVariable.h"

namespace AutomationX
{
	public ref class AXInstance
	{
	private:
		AX^ _ax;
		ManagedTypeConverter _converter;
		volatile UInt32 _pollingInterval = 5000;
		volatile bool _stopWorkerTimer = false;
		Timers::Timer^ _workerTimer;
		String^ _name = "";
		AXVariable^ _statusVariable = nullptr;
		AXVariable^ _alarmVariable = nullptr;
		AXVariable^ _alarmTextVariable = nullptr;
		String^ _statusText = "";
		String^ _errorText = "";
		Mutex _variableListMutex;
		List<String^>^ _variableNames = gcnew List<String^>();
		List<AXVariable^>^ _variableList = gcnew List<AXVariable^>();
		Dictionary<String^, AXVariable^>^ _variables = gcnew Dictionary<String^, AXVariable^>();

		void GetVariables();
		void Worker(System::Object ^sender, System::Timers::ElapsedEventArgs ^e);
	public:
		delegate void StatusEventHandler(AXInstance^ sender, String^ statusText);
		delegate void ErrorEventHandler(AXInstance^ sender, String^ errorText);

		/// <summary>Fired when aX status variable provided with constructor is being set.</summary>
		event StatusEventHandler^ OnStatus;
		/// <summary>Fired when aX alarm variable provided with constructor is being set.</summary>
		event ErrorEventHandler^ OnError;

		property AX^ AutomationX { AX^ get() { return _ax; } }
		property String^ Name { String^ get() { return _name; } }
		property String^ Remark { String^ get(); }
		property String^ Status { String^ get() { return _statusText; } void set(String^ value); }
		property String^ Error { String^ get() { return _errorText; } void set(String^ value); }

		/// <summary>Sets the worker threads polling interval in milliseconds. Only used when events are enabled.</summary>
		property UInt32 PollingInterval { UInt32 get() { return _pollingInterval; } void set(UInt32 value) { _pollingInterval = value; } }

		/// <summary>Returns a collection of all variables.</summary>
		property array<AXVariable^>^ Variables { array<AXVariable^>^ get(); }

		/// <summary>Returns the aX variable of the specified name.</summary>
		/// <param name='variableName'>The name of the variable.</param>
		/// <return>Returns an aX variable object or null, when the variable was not found.</return>
		property AXVariable^ default[String^] { AXVariable^ get(String^ variableName); }
		
		/// <summary>Constructor</summary>
		/// <param name='ax'>The aX object.</param>
		/// <param name='name'>Name of this aX instance.</param>
		AXInstance(AX^ ax, String^ name);

		/// <summary>Constructor taking names of status variables.</summary>
		/// <param name='ax'>The aX object.</param>
		/// <param name='name'>Name of this aX instance.</param>
		/// <param name='statusVariableName'>Name of a status variable of type STRING to store status information.</param>
		/// <param name='alarmVariableName'>Name of a variable of type ALARM.</param>
		AXInstance(AX^ ax, String^ name, String^ statusVariableName, String^ alarmVariableName);

		virtual ~AXInstance();

		/// <summary>Returns the aX variable of the specified name.</summary>
		/// <param name='variableName'>The name of the variable.</param>
		/// <return>Returns an aX variable object or null, when the variable was not found.</return>
		AXVariable^ Get(String^ variableName);

		/// <summary>Checks if a variable exists.</summary>
		/// <returns>True when the variable name was found, otherwise false.</returns>
		bool VariableExists(String^ variableName);

		void EnableVariableEvents();
		void DisableVariableEvents();
	};
}

