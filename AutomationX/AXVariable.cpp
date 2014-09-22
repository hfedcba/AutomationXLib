#include "stdafx.h"
#include "AXVariable.h"
#include "AXInstance.h"

namespace AutomationX
{
	bool AXVariable::Events::get()
	{
		return _events;
	}

	void AXVariable::Events::set(bool value)
	{
		_events = value;
		if (_events) _instance->RegisterVariableToPoll(this);
		else _instance->UnregisterVariableToPoll(this);
	}

	bool AXVariable::Retentive::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetRetentiveFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::Constant::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetConstantFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::Private::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetPrivateFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::Local::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetLocalFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::ConfigurationValue::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetConfValueFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::Parameter::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetParameterFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::Remote::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetRemoteFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	bool AXVariable::NotConnected::get()
	{
		if(!HandleSpsIdChange()) return false;
		int result = AxGetNcFlag(_execData);
		if (result == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		if (result == 0) return false;
		return true;
	}

	String^ AXVariable::ReferenceName::get()
	{
		if(!HandleSpsIdChange()) return "";
		const char* reference = AxGetVarReference(_execData);
		if (!reference) throw gcnew AXVariableException("The data handle is invalid, the variable is not a reference type variable or the variable is not connected.");
		return gcnew String(reference);
	}

	AXVariableDeclaration AXVariable::Declaration::get()
	{
		if(!HandleSpsIdChange()) return AXVariableDeclaration::axVar;
		int declaration = AxGetVarDeclaration(_execData);
		if (declaration == -1) throw gcnew AXVariableException("The data handle is invalid.");
		return (AXVariableDeclaration)declaration;
	}

	String^ AXVariable::Remark::get()
	{
		if(!HandleSpsIdChange()) return "";
		String^ value = gcnew String(AxGetRemark(_execData));
		return value;
	}

	/*void AXVariable::Remark::set(String^ value)
	{
		_ax->SpsIdChanged();
		static AX_EXEC_DATA execData;
		int result = AxQueryVariable(_cName, &execData);
		if (!result) throw gcnew AXVariableException("Variable or object was not found.");
		std::string temp = _converter.GetString(value);
		char* remark = new char[temp.size() + 1];
		strcpy_s(remark, temp.size() + 1, temp.c_str());
		if (!AxSetRemark(&execData, remark))
		{
			delete[] remark;
			throw gcnew AXVariableException("The variable was not found or the name was found but not a variable type.");
		}
		delete[] remark;
	}*/

	String^ AXVariable::Path::get()
	{
		return _instance->Path + "." + _name;
	}

	UInt16 AXVariable::Length::get()
	{
		if(!HandleSpsIdChange()) return false;
		return AxGetArrayCnt(_execData);
	}

	AXVariable::AXVariable(AXInstance^ instance, String^ name)
	{
		_instance = instance;
		_name = name;
		_cName = _converter.GetCString(Path);
		_ax = instance->AutomationX;
		Int32 _spsId = _ax->SpsId;
		GetExecData();
		Refresh();
	}

	AXVariable::~AXVariable()
	{
		if (_cName) Marshal::FreeHGlobal(IntPtr((void*)_cName)); //Always free memory! Don't remove this here! There is a memory leak, when this line is only in the finalizer
		_cName = nullptr;
		_ax = nullptr;
		_instance = nullptr;
		if (_boolValues) _boolValues->Clear();
		_boolValues = nullptr;
		if (_integerValues) _integerValues->Clear();
		_integerValues = nullptr;
		if (_stringValues) _stringValues->Clear();
		_stringValues = nullptr;
		if (_realValues) _realValues->Clear();
		_realValues = nullptr;
		if (_execData) AxFreeExecData(_execData);
		_execData = nullptr;
		//GC::Collect(); //Uncomment to check for memory leaks
	}

	AXVariable::!AXVariable()
	{
		if (_cName) Marshal::FreeHGlobal(IntPtr((void*)_cName)); //Always free memory!
		if (_execData) AxFreeExecData(_execData);
	}

	bool AXVariable::HandleSpsIdChange()
	{
		if (_spsId != _ax->CheckSpsId())
		{
			try
			{
				_spsIdChangeMutex.WaitOne();
				//Don't call AxFreeExecData as it causes writing into invalid memory locations
				//if (_execData) AxFreeExecData(_execData);
				_execData = nullptr;
				char* cName = _converter.GetCString(Path);
				void* handle = AxQueryExecDataEx(cName);
				Marshal::FreeHGlobal(IntPtr((void*)cName)); //Always free memory!
				if (!handle)
				{
					_spsIdChangeMutex.ReleaseMutex();
					return false;
				}
				AxFreeExecData(handle);
				_spsId = _ax->SpsId;
				GetExecData();
				_spsIdChangeMutex.ReleaseMutex();
				Refresh();
			}
			catch (const Exception^ ex)
			{
				_spsIdChangeMutex.ReleaseMutex();
				throw ex;
			}
		}
		return true;
	}

	void AXVariable::GetExecData()
	{
		if (_execData) AxFreeExecData(_execData);
		_execData = AxQueryExecDataEx(_cName);
		if (!_execData) throw gcnew AXVariableException("The variable was not found.");
	}

	int AXVariable::GetRawType()
	{
		if(!HandleSpsIdChange()) return false;
		return AxGetType(_execData);
	}

	void AXVariable::Clear()
	{
		int type = 0;
		try
		{
			type = GetRawType();
		}
		catch (const AXVariableException^)
		{
			throw gcnew AXVariableTypeException("Could not determine type of variable " + Path + ".");
		}
		if (type == AX_BT_BOOL || type == AX_BT_ALARM)
		{
			_boolValues = gcnew List<bool>(_length);
			for (int i = 0; i < _length; i++) _boolValues->Add(false);
		}
		else if (type == AX_BT_BYTE || type == AX_BT_SINT || type == AX_BT_INT || type == AX_BT_DINT || type == AX_BT_USINT || type == AX_BT_UINT || type == AX_BT_UDINT)
		{
			_integerValues = gcnew List<Int64>(_length);
			for (int i = 0; i < _length; i++) _integerValues->Add(0);
		}
		else if (type == AX_BT_REAL || type == AX_BT_LREAL)
		{
			_realValues = gcnew List<Double>(_length);
			for (int i = 0; i < _length; i++) _realValues->Add(0);
		}
		else if (type == AX_BT_STRING)
		{
			_stringValues = gcnew List<String^>(_length);
			for (int i = 0; i < _length; i++) _stringValues->Add("");
		}
		else throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " of instance " + _instance->Path + " has unknown type.");
	}

	void AXVariable::Refresh()
	{
		Refresh(false);
	}

	void AXVariable::Refresh(bool raiseEvents)
	{
		if (!raiseEvents)
		{
			try
			{
				_length = Length;
			}
			catch (const AXVariableException^) {}
			if (_length == 0) _length = 1;
			_isArray = _length > 1;
			Clear();
			GetAttributes();
		}
		for (UInt16 i = 0; i < _length; i++)
		{
			Refresh(i, raiseEvents);
		}
	}

	void AXVariable::Refresh(UInt16 index)
	{
		Refresh(index, false);
	}

	void AXVariable::Refresh(UInt16 index, bool raiseEvents)
	{
		if(!HandleSpsIdChange()) return;
		if (index >= _length) throw gcnew AXArrayIndexOutOfRangeException("The index exceeds the array boundaries.");
		struct tagAxVariant data;
		try
		{
			int result = _isArray ? AxGetArray(_execData, &data, index) : AxGet(_execData, &data);
			if (!result) throw (AXException^)(gcnew AXVariableException("The data handle is invalid or does not represent a variable type."));
		}
		//TODO: Handle AccessViolationException everywhere
		catch (const AccessViolationException^ ex)
		{
			return;
		}
		bool valueChanged = false;
		if (data.ucVarType == AX_BT_BOOL || data.ucVarType == AX_BT_ALARM)
		{
			_type = (AXVariableType)data.ucVarType;
#pragma warning (disable: 4800)
			bool newValue = data.AXVAL.btBOOL;
#pragma warning (default: 4800)
			if (_boolValues[index] != newValue)
			{
				_boolValues[index] = newValue;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_BYTE || data.ucVarType == AX_BT_USINT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btBYTE)
			{
				_integerValues[index] = data.AXVAL.btBYTE;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_SINT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btSINT)
			{
				_integerValues[index] = data.AXVAL.btSINT;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_INT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btINT)
			{
				_integerValues[index] = data.AXVAL.btINT;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_DINT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btDINT)
			{
				_integerValues[index] = data.AXVAL.btDINT;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_UINT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btUINT)
			{
				_integerValues[index] = data.AXVAL.btUINT;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_UDINT)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_integerValues[index] != data.AXVAL.btUDINT)
			{
				_integerValues[index] = data.AXVAL.btUDINT;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_REAL)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_realValues[index] != data.AXVAL.btREAL)
			{
				_realValues[index] = data.AXVAL.btREAL;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_LREAL)
		{
			_type = (AXVariableType)data.ucVarType;
			if (_realValues[index] != data.AXVAL.btLREAL)
			{
				_realValues[index] = data.AXVAL.btLREAL;
				valueChanged = true;
			}
		}
		else if (data.ucVarType == AX_BT_STRING)
		{
			_type = (AXVariableType)data.ucVarType;
			String^ newValue = gcnew String(data.AXVAL.btSTRING);
			if (_stringValues[index] != newValue)
			{
				_stringValues[index] = newValue;
				valueChanged = true;
			}
		}
		else
		{
			throw gcnew AXVariableTypeException("The type of the variable is unknown.");
		}
		if (raiseEvents && valueChanged)
		{
			if (_isArray) ArrayValueChanged(this, index); else ValueChanged(this);
		}
	}

	void AXVariable::Set(tagAxVariant& data, UInt16 index)
	{
		if(!HandleSpsIdChange()) return;
		int result = _isArray ? AxSetArray(_execData, &data, index) : AxSet(_execData, &data);
		if (!result) throw gcnew AXVariableException("The data handle is invalid or does not represent a variable type.");
	}

	bool AXVariable::GetBool()
	{
		if(!HandleSpsIdChange()) return false;
		if (_type != AXVariableType::axBool && _type != AXVariableType::axAlarm) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BOOL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return _boolValues[0];
	}

	void AXVariable::Set(bool value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axBool && _type != AXVariableType::axAlarm) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BOOL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btBOOL = value;
		_boolValues[0] = value;
		Set(data, 0);
	}

	bool AXVariable::GetBool(UInt16 index)
	{
		if(!HandleSpsIdChange()) return false;
		if (_type != AXVariableType::axBool && _type != AXVariableType::axAlarm) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BOOL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return _boolValues[index];
	}

	void AXVariable::Set(UInt16 index, bool value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axBool && _type != AXVariableType::axAlarm) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BOOL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btBOOL = value;
		_boolValues[index] = value;
		Set(data, index);
	}

	Byte AXVariable::GetByte()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axByte && _type != AXVariableType::axUnsignedShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BYTE.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (Byte)_integerValues[0];
	}

	void AXVariable::Set(Byte value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axByte && _type != AXVariableType::axUnsignedShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BYTE.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btBYTE = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	Byte AXVariable::GetByte(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axByte && _type != AXVariableType::axUnsignedShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BYTE.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (Byte)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, Byte value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axByte && _type != AXVariableType::axUnsignedShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type BYTE.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btBYTE = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	char AXVariable::GetShortInteger()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type INT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (char)_integerValues[0];
	}

	void AXVariable::Set(char value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type SINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btSINT = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	char AXVariable::GetShortInteger(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type SINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (char)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, char value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axShortInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type SINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btSINT = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	Int16 AXVariable::GetInteger()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type SINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (Int16)_integerValues[0];
	}

	void AXVariable::Set(Int16 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type INT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btINT = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	Int16 AXVariable::GetInteger(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type INT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (Int16)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, Int16 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type INT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btINT = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	Int32 AXVariable::GetLongInteger()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type DINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (Int32)_integerValues[0];
	}

	void AXVariable::Set(Int32 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type DINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btDINT = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	Int32 AXVariable::GetLongInteger(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type DINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (Int32)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, Int32 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type DINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btDINT = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	UInt16 AXVariable::GetUnsignedInteger()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axUnsignedInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (UInt16)_integerValues[0];
	}

	void AXVariable::Set(UInt16 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axUnsignedInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btUINT = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	UInt16 AXVariable::GetUnsignedInteger(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axUnsignedInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (UInt16)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, UInt16 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axUnsignedInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btUINT = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	UInt32 AXVariable::GetUnsignedLongInteger()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axUnsignedLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UDINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (UInt32)_integerValues[0];
	}

	void AXVariable::Set(UInt32 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axUnsignedLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UDINT.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btUDINT = value;
		_integerValues[0] = value;
		Set(data, 0);
	}

	UInt32 AXVariable::GetUnsignedLongInteger(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axUnsignedLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UDINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (UInt32)_integerValues[index];
	}

	void AXVariable::Set(UInt16 index, UInt32 value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axUnsignedLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type UDINT.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btUDINT = value;
		_integerValues[index] = value;
		Set(data, index);
	}

	Single AXVariable::GetReal()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return (Single)_realValues[0];
	}

	void AXVariable::Set(Single value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btREAL = value;
		_realValues[0] = value;
		Set(data, 0);
	}

	Single AXVariable::GetReal(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return (Single)_realValues[index];
	}

	void AXVariable::Set(UInt16 index, Single value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btREAL = value;
		_realValues[index] = value;
		Set(data, index);
	}

	Double AXVariable::GetLongReal()
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axLongReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return _realValues[0];
	}

	void AXVariable::Set(Double value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axLongReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btLREAL = value;
		_realValues[0] = value;
		Set(data, 0);
	}

	Double AXVariable::GetLongReal(UInt16 index)
	{
		if(!HandleSpsIdChange()) return 0;
		if (_type != AXVariableType::axLongReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return _realValues[index];
	}

	void AXVariable::Set(UInt16 index, Double value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axLongReal) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type REAL.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		data.AXVAL.btLREAL = value;
		_realValues[index] = value;
		Set(data, index);
	}

	String^ AXVariable::GetString()
	{
		if(!HandleSpsIdChange()) return "";
		if (_type != AXVariableType::axString) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type STRING.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		Refresh((UInt16)0);
		return _stringValues[0];
	}

	void AXVariable::Set(String^ value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axString) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type STRING.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		strcpy_s(data.AXVAL.btSTRING, _converter.GetString(value).c_str());
		_stringValues[0] = value;
		Set(data, 0);
	}

	String^ AXVariable::GetString(UInt16 index)
	{
		if(!HandleSpsIdChange()) return "";
		if (_type != AXVariableType::axString) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type STRING.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		Refresh((UInt16)index);
		return _stringValues[index];
	}

	void AXVariable::Set(UInt16 index, String^ value)
	{
		if(!HandleSpsIdChange()) return;
		if (_type != AXVariableType::axString) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type STRING.");
		if (!_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is no array.");
		struct tagAxVariant data;
		data.ucVarType = (unsigned char)_type;
		strcpy_s(data.AXVAL.btSTRING, _converter.GetString(value).c_str());
		_stringValues[index] = value;
		Set(data, index);
	}

	void AXVariable::GetAttributes()
	{
		if(!HandleSpsIdChange()) return;
		AX_ATTR attrs;
		if (AxGetAttributes(_execData, &attrs) == -1) throw gcnew AXVariableException("The variable data handle is invalid.");
		_decimalPoints = attrs.dec_point;
		if (attrs.dim) _dimension = gcnew String(attrs.dim);
		if (attrs.rem == 0) _global = false; else _global = true;
		if (attrs.trend == 0) _trending = false; else _trending = true;
	}

	String^ AXVariable::GetEnumRemark(Int32 enumValue)
	{
		if (!HandleSpsIdChange()) return "";
		if (_type != AXVariableType::axLongInteger) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is not of type ENUM.");
		if (_isArray) throw gcnew AXVariableTypeException("Variable " + _instance->Path + "." + _name + " is an array. Please specify the element index.");
		char buffer[50] = "";
		AxGetEnumText(_execData, enumValue, buffer, 50);
		return gcnew String(buffer);
	}
}
