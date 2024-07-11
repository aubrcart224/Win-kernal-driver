#include <ntifs.h>
//#include <header_files.h>


extern "C" {
	//makes driver compatible with kd mapper 
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName, 
													PDRIVER_INITIALIZE InitializationFunction);
	

	//implent read andn write to process memory insdier driver 
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS FromProcess, 
											 PVOID FromAddress, 
											 PEPROCESS ToProcess, 
											 PVOID ToAddress, 
											 SIZE_T BufferSize, 
											 KPROCESSOR_MODE PreviousMode, 
											 PSIZE_T NumberOfBytesCopied);

}


void debug_print(PCSTR text) {

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}


//namespace driver 
namespace driver {
	namespace codes {
			constexpr ULONG attach = 
				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

			constexpr ULONG read =
				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

			constexpr ULONG write =
				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		
	}

	//shared between user and kernel space
	struct Request {

		HANDLE process_id;

		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T return_size;

	};

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}
	
	//to do  
	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);

		debug_print("[+] Device control called.\n");

		NTSTATUS status = STATUS_UNSUCCESSFUL;

		// we need this ot detrime which code was passed through. 
		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);

		//access the request object sent from user mode. 
		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);

		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status; 
		}



		//Target process we want to R&W memory to. in
		static PEPROCESS target_process = nullptr;

		//switch case to determine which code was passed through.
		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode;	
		switch (control_code) {
			case codes::attach: {
				//attach to process 
				status = PsLookupProcessByProcessId(request->process_id, &target_process); //why error here? :(,  fixed :) 
				break;
			}
			case codes::read: {
				//read process memory 
				if (target_process == nullptr) {
					break;
				}

				//copy memory from target process to our buffer 
				status = MmCopyVirtualMemory(target_process, request->target, PsGetCurrentProcess(), request->buffer, request->size, KernelMode, &request->return_size);
				break;
			}
			case codes::write: {
				//write process memory 
				if (target_process == nullptr) {
					break;
				}

				//copy memory from our buffer to target process 
				status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, target_process, request->target, request->size, KernelMode, &request->return_size);
				break;
			}
		}

		
		irp->IoStatus.Status = status;
		irp->IoStatus.Information = sizeof(Request);
		return status;


		//IoCompleteRequest(irp, IO_NO_INCREMENT);


		//return irp->IoStatus.Status;
	}

}	


NTSTATUS driver_main(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {

	UNREFERENCED_PARAMETER(registry_path);

	UNICODE_STRING device_name = {};
	RtlInitUnicodeString(&device_name, L"\\Device\\KM"); 

	//create driver device obj. 
	PDEVICE_OBJECT device_object = nullptr;	
	NTSTATUS status = IoCreateDevice(driver_object, 0, &device_name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device_object);

	if (status != STATUS_SUCCESS) {
		debug_print("[-] failed to create driver device\n");
		return status;
	}
	debug_print("[+] created driver device\n");

	//might error later check if things dont work 
	UNICODE_STRING symbolic_link = {};
	RtlInitUnicodeString(&symbolic_link, L"\\DosDevices\\KM");

	status = IoCreateSymbolicLink(&symbolic_link, &device_name);
	if (status != STATUS_SUCCESS) {
		debug_print("[-] failed to create symbolic link\n");
		return status;
	}

	debug_print("[+] Driver symbolic link succesfully established.\n");
	

	// allow to send small amounts of data between um/km
	SetFlag(device_object->Flags, DO_BUFFERED_IO);


	//good now? (handlers) 
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;

	//We have initalized our device.
    ClearFlag(device_object->Flags, DO_DEVICE_INITIALIZING);	

	debug_print("[+] Driver initalized successfully\n");


	return status;
	
}


// KDmapper will call this "entry point" but params will be null 
NTSTATUS DriverEntry() {

	debug_print("[+] hello kernel\n");

	//might error on print here 
	UNICODE_STRING driver_name = {};
	RtlInitUnicodeString(&driver_name, L"\\Device\\KM");

	return IoCreateDriver(&driver_name, &driver_main);

}