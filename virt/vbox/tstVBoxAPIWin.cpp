/* $Id: tstVBoxAPIWin.cpp 92072 2014-02-06 10:30:07Z klaus $ */
/** @file
 *
 * tstVBoxAPIWin - sample program to illustrate the VirtualBox
 *                 COM API for machine management on Windows.
                   It only uses standard C/C++ and COM semantics,
 *                 no additional VBox classes/macros/helpers. To
 *                 make things even easier to follow, only the
 *                 standard Win32 API has been used. Typically,
 *                 C++ developers would make use of Microsoft's
 *                 ATL to ease development.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * PURPOSE OF THIS SAMPLE PROGRAM
 * ------------------------------
 *
 * This sample program is intended to demonstrate the minimal code necessary
 * to use VirtualBox COM API for learning puroses only. The program uses pure
 * Win32 API and doesn't have any extra dependencies to let you better
 * understand what is going on when a client talks to the VirtualBox core
 * using the COM framework.
 *
 * However, if you want to write a real application, it is highly recommended
 * to use our MS COM XPCOM Glue library and helper C++ classes. This way, you
 * will get at least the following benefits:
 *
 * a) better portability: both the MS COM (used on Windows) and XPCOM (used
 *    everywhere else) VirtualBox client application from the same source code
 *    (including common smart C++ templates for automatic interface pointer
 *    reference counter and string data management);
 * b) simpler XPCOM initialization and shutdown (only a single method call
 *    that does everything right).
 *
 * Currently, there is no separate sample program that uses the VirtualBox MS
 * COM XPCOM Glue library. Please refer to the sources of stock VirtualBox
 * applications such as the VirtualBox GUI frontend or the VBoxManage command
 * line frontend.
 */


#include <stdio.h>
#include "VirtualBox.h"
#include <atlbase.h>
#include <atlcom.h>

#define SAFE_RELEASE(x) \
    if (x) { \
        x->Release(); \
        x = NULL; \
    }

int listVMs(IVirtualBox *virtualBox)
{
    HRESULT rc;

    SAFEARRAY *machinesArray = NULL;

    rc = virtualBox->get_Machines(&machinesArray);
    if (SUCCEEDED(rc))
    {
        IMachine **machines;
        rc = SafeArrayAccessData(machinesArray, (void **) &machines);
        if (SUCCEEDED(rc))
        {
            for (ULONG i = 0; i < machinesArray->rgsabound[0].cElements; ++i)
            {
                CComBSTR str;
                rc = machines[i]->get_Name(&str);
                if (SUCCEEDED(rc))
                {
                    printf("Name: %S\n", str);
                }
            }

            SafeArrayUnaccessData(machinesArray);
        }

        SafeArrayDestroy(machinesArray);
    }

    return 0;
}


int testStartVM(IVirtualBox *virtualBox)
{
    HRESULT rc;

    CComPtr<IMachine> machine;
    CComBSTR machineName(L"Windows XP");

    rc = virtualBox->FindMachine(machineName, &machine);

    if (FAILED(rc))
    {
		CComPtr<IErrorInfo> errorInfo;

        rc = GetErrorInfo(0, &errorInfo);

        if (FAILED(rc))
            printf("Error getting error info! rc = 0x%x\n", rc);
        else
        {
            CComBSTR errorDescription;

            rc = errorInfo->GetDescription(&errorDescription);

            if (FAILED(rc) || !errorDescription)
                printf("Error getting error description! rc = 0x%x\n", rc);
            else
            {
                printf("Successfully retrieved error description: %S\n", errorDescription);
            }
        }
    }
    else
    {
        CComPtr<ISession> session;
        CComPtr<IConsole> console;
        CComPtr<IProgress> progress;
        CComBSTR sessiontype(L"gui");

        do
        {
			CComBSTR guid;

            rc = machine->get_Id(&guid);
            if (!SUCCEEDED(rc))
            {
                printf("Error retrieving machine ID! rc = 0x%x\n", rc);
                break;
            }
			
			rc = session.CoCreateInstance(CLSID_Session);

            if (!SUCCEEDED(rc))
            {
                printf("Error creating Session instance! rc = 0x%x\n", rc);
                break;
            }

            rc = machine->LaunchVMProcess(session, sessiontype, nullptr, &progress);
            if (!SUCCEEDED(rc))
            {
                printf("Could not open remote session! rc = 0x%x\n", rc);
                break;
            }

            /* Wait until VM is running. */
            printf("Starting VM, please wait ...\n");
            rc = progress->WaitForCompletion(-1);

            /* Get console object. */
            session->get_Console(&console);

            /* Bring console window to front. */
            machine->ShowConsoleWindow(0);

            printf("Press enter to power off VM and close the session...\n");
            getchar();

            /* Power down the machine. */
            rc = console->PowerDown(&progress);

            /* Wait until VM is powered down. */
            printf("Powering off VM, please wait ...\n");
            rc = progress->WaitForCompletion(-1);

            /* Close the session. */
            rc = session->UnlockMachine();

        } while (0);
    }

    return 0;
}


int testVBoxAPI()
{
    HRESULT rc;
    CComPtr<IVirtualBox> virtualBox;

    do
    {
        CoInitialize(nullptr);

		rc = virtualBox.CoCreateInstance(CLSID_VirtualBox);
        /* Instantiate the VirtualBox root object. */
       // rc = CoCreateInstance(CLSID_VirtualBox,       /* the VirtualBox base object */
        //                      NULL,                   /* no aggregation */
       //                       CLSCTX_LOCAL_SERVER,    /* the object lives in a server process on this machine */
        //                      IID_IVirtualBox,        /* IID of the interface */
        //                      (void**)&virtualBox);

        if (!SUCCEEDED(rc))
        {
            printf("Error creating VirtualBox instance! rc = 0x%x\n", rc);
            break;
        }

        listVMs(virtualBox);
		testStartVM(virtualBox);
    } while (0);

    CoUninitialize();
    return 0;
}

