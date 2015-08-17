#include "vboxcore.h"

const QString vboxErrorString(HRESULT rc, const QString& def = QString()) {
	static const QHash<HRESULT, QString> hash {
		{ 0x80BB0001, "VBOX_E_OBJECT_NOT_FOUND: Object corresponding to the supplied arguments does not exist" },
		{ 0x80BB0002, "VBOX_E_INVALID_VM_STATE: Current virtual machine state prevents the operation" },
		{ 0x80BB0003, "VBOX_E_VM_ERROR: Virtual machine error occurred attempting the operation" },
		{ 0x80BB0004, "VBOX_E_FILE_ERROR: File not accessible or erroneous file contents" },
		{ 0x80BB0005, "VBOX_E_IPRT_ERROR: Runtime subsystem error" },
		{ 0x80BB0006, "VBOX_E_PDM_ERROR: Pluggable Device Manager error" },
		{ 0x80BB0007, "VBOX_E_INVALID_OBJECT_STATE: Current object state prohibits operation" },
		{ 0x80BB0008, "VBOX_E_HOST_ERROR: Host operating system related error" },
		{ 0x80BB0009, "VBOX_E_NOT_SUPPORTED: Requested operation is not supported" },
		{ 0x80BB000A, "VBOX_E_XML_ERROR: Invalid XML found" },
		{ 0x80BB000B, "VBOX_E_INVALID_SESSION_STATE: Current session state prohibits operation" },
		{ 0x80BB000C, "VBOX_E_OBJECT_IN_USE: Object being in use prohibits operation" },
		{ 0x80BB000D, "VBOX_E_PASSWORD_INCORRECT: A provided password was incorrect" },
		{ E_INVALIDARG, "E_INVALIDARG: Value of the method's argument is not within the range of valid values" },
		{ E_POINTER, "E_POINTER: Pointer for the output argument is invalid" },
		{ E_ACCESSDENIED, "E_ACCESSDENIED: Called object is not ready" },
		{ E_OUTOFMEMORY, "E_OUTOFMEMORY: Memory allocation operation fails" }
	};

	return hash.value(rc, def);
};

class ResultStorage {
public:
	ResultStorage(HRESULT code, const QString& str) : code_(code), str_(str) {}

	ResultStorage() : ResultStorage(S_OK, QString()) {}

	HRESULT code() const { return code_; }

	QString str() const { return str_; }

private:
	HRESULT code_;
	QString str_;
};

QThreadStorage<ResultStorage>& resultStorage() {
	static QThreadStorage<ResultStorage> error;
	return error;
}

HRESULT vboxLastError() {
	return resultStorage().localData().code();
}

QString vboxLastErrorString() {
	return resultStorage().localData().str();
}

bool vboxCheck(HRESULT rc) {
	const auto ok = SUCCEEDED(rc);
	QString errorString;

	if (!ok) {
		IErrorInfo* errorInfo = nullptr;
		if (FAILED(GetErrorInfo(0, &errorInfo))) {
			errorInfo = nullptr;
		}
		_com_error error(rc, errorInfo);

		errorString += vboxErrorString(rc);
		if(errorString.isEmpty()) {
			errorString += toString(error.ErrorMessage());
		}

		if (errorInfo) {
			const auto addInfomation = [&errorString](
				const char* prefix,
				const wchar_t* info
				) {
				const auto infoString = toString(info);
				if (!infoString.isEmpty()) {
					errorString += prefix;
					errorString += infoString;
				}
			};

			addInfomation(" \nDesciption : ", error.Description());
			addInfomation(" \nSource : ", error.Source());
			addInfomation(" \nGUID : ", CComBSTR(error.GUID()));
			addInfomation(" \nHelpFile : ", error.HelpFile());
		}
	}

	resultStorage().setLocalData({ rc, errorString });

	return ok;
}

