/*
	ebtl.cpp
	e(xplorer)b(ackground)t(ool)l(oader)
	Installa/carica/scarica la DLL ExplorerBgToolRe(dux).
	Unicode esplicito.
	Luca Piergentili, 18/06/26

	La DLL originale (ExplorerBgTool) solo funziona in modo x64, in x86 nonostante la correzione di alcuni errori e le
	varie pezze applicate non funziona. Il progetto relativo e' Unicode nativo.
	Qui usa la versione Redux della DLL, che oltre ai vari bugfix e modifiche, permette la registrazione via codice, non
	solo tramite runsvr32.exe come la originale.

	Questo progetto e' per x64 (come la DLL) ed e' di tipo Multibyte (NO Unicode), dato che usa codice di libreria ANSI,
	non compatibile. Quando necessita usare Unicode, la fa quindi in modo esplicito, non tramite la interfaccia TCHAR.

	Dato che si tratta di un progetto console, non incorpora nativamente MFC/ATL etc., quindi l'inizializzazione della
	interfaccia COM deve essere fatta in modo esplicito.

	Il programma va compilato in modalita' statica: Progetto->Proprieta'->C/C++->Generazione codice->Libreria x run-time
	(switch /MT o /MTd per DEBUG), in modo tale che l'eseguibile contenga tutto il codice di libreria necessario per
	girare senza bisogno di generare un installatore che contenga i ridistribuibili di run-time.
*/
#include "pragma.h"
#include "window.h"
#include "win32api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "strings.h"
#include <conio.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <shellapi.h>
#include <objbase.h>
#include "CExplorerBgToolRe.h"
#include "L:/ebtl/version.h"
#include "resource.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACECONSOLE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define EBTL_PROJECT_HOME				L"https://github.com/lpierge/ebtl"
#define EXPLORERBGTOOLRE_PROJECT_HOME	L"https://github.com/lpierge/ExplorerBgToolRe"

/*
	ACTIONTYPE
*/
typedef enum action_t {
	EBTL_INSTALL,
	EBTL_REGISTER,
	EBTL_UNREGISTER,
	EBTL_RELOAD_DLL,
	EBTL_RESTART_EXPLORER,
	EBTL_STATUS
} ACTIONTYPE;

/*
	ElevateAndRestart()

	Riavvia il programma con i privilegi da amministratore.
	(la registrazione/rimozione della DLL richiede permessi da admin)
*/
BOOL ElevateAndRestart(int argc,wchar_t* argv[])
{
	wchar_t szPath[_MAX_PATH+1] = {0};
	if(::GetModuleFileNameW(NULL,szPath,_MAX_PATH)==0)
		return(FALSE);

	wchar_t szDir[_MAX_PATH+1] = {0};
	::GetCurrentDirectoryW(_MAX_PATH,szDir);

	// ricostruisce la riga di comando per il nuovo processo elevato
	// ignora argv[0] (l'eseguibile), parte da argv[1]
	wchar_t szParams[1024] = L"";
	for(int i=1; i < argc; ++i)
	{
		wcscat_s(szParams,1024,L"\"");	// mette l'argomento tra virgolette
		wcscat_s(szParams,1024,argv[i]);
		wcscat_s(szParams,1024,L"\" ");	// spazio separatore
	}

	// si riesegue
	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.fMask = SEE_MASK_DEFAULT; 
	sei.lpVerb = L"runas";
	sei.lpFile = szPath;
	sei.lpParameters = szParams; // passa gli argomenti recuperati
	sei.lpDirectory = szDir;
	sei.nShow = SW_SHOWNORMAL;

	return(::ShellExecuteExW(&sei));
}

/*
	ExtractDLL()
*/
BOOL ExtractDLL(const char* lpcszOutputName)
{
	BOOL bRet = ExtractResource(IDR_DLL_FILE,RT_RCDATA,lpcszOutputName);

	return(bRet);
}

/*
	ExtractResources()
*/
void ExtractResources(const char* lpcszInstallDir,BOOL& bAllResExtracted)
{
	typedef struct resource_t {
		char szName[MAX_PATH+1];
		UINT nId;
	} RESOURCEDATA;

	char szResource[_MAX_PATH+1] = {0};
	static const RESOURCEDATA resources_array[] = {
		{"config.ini",			IDR_CONFIG_INI},
		{"Image\\image.png",	IDR_IMAGE_PNG},
		{"Image\\image01.jpg",	IDR_IMAGE01_JPG},
		{"Image\\image02.jpg",	IDR_IMAGE02_JPG},
		{"Image\\image03.jpg",	IDR_IMAGE03_JPG},
		{"Image\\image04.jpg",	IDR_IMAGE04_JPG},
		{"Image\\image05.jpg",	IDR_IMAGE05_JPG},
		{"Image\\image06.jpg",	IDR_IMAGE06_JPG},
		{"Chibi\\chibi01.png",	IDR_CHIBI01_PNG},
		{"Chibi\\chibi02.jpg",	IDR_CHIBI02_JPG},
		{"Chibi\\chibi03.jpg",	IDR_CHIBI03_JPG},
		{"Chibi\\chibi04.jpg",	IDR_CHIBI04_JPG},
		{"Chibi\\chibi05.jpg",	IDR_CHIBI05_JPG},
		{"Chibi\\notchibi.png",	IDR_NOTCHIBI_PNG}
	};

	bAllResExtracted = TRUE;

	// preserva l'eventuale config.ini gia' esistente
	snprintf(szResource,sizeof(szResource),"%s\\%s",lpcszInstallDir,resources_array[0].szName);
	char szYetAnotherFile[_MAX_PATH+1] = {0};
	char* pNewName = YetAnotherFileName(szResource,szYetAnotherFile,sizeof(szYetAnotherFile));
	if(pNewName)
	{
		wchar_t wzError[1024] = {0};
		::MoveFileA(szResource,szYetAnotherFile);
		_snwprintf(wzError,_countof(wzError)-1,L"Warning: the existing config.ini file has been renamed to %S to avoid being overwritten by the default config.ini file from the new installation.\nYou have to manually update the new config.ini file with the previous configuration values.",szYetAnotherFile);
		wprintf(wzError);
		::MessageBeep(MB_ICONWARNING);
		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
	}

	// estrae tutte le risorse
	for(int i=0; i < ARRAY_SIZE(resources_array); i++)
	{
		snprintf(szResource,sizeof(szResource),"%s\\%s",lpcszInstallDir,resources_array[i].szName);
		if(!FileExists(szResource))
			if(!ExtractResource(resources_array[i].nId,RT_RCDATA,szResource))
				bAllResExtracted = FALSE;
	}
}

/*
	wmain()
*/
int wmain(int argc,wchar_t* argv[])
{
	HRESULT hr = NULL;
	BOOL bCoInitialized = FALSE;
	action_t eAction = EBTL_STATUS;
	wchar_t wzDllPath[_MAX_PATH+1] = {0};
	wchar_t wzInstallDir[_MAX_PATH+1] = {L"C:\\ExplorerBgToolRe"};
	wchar_t argument[_MAX_PATH+1] = {0};
    CExplorerBgToolRe explorerBgToolRe;
	wchar_t wzError[256] = {0};

	setlocale(LC_ALL,"");

	InitConsoleGeometry(120,9000);

	wprintf(L"%s v%d.%d.%d (%s)\n"\
			"e(xplorer)b(ackground)t(ool)l(oader).\n"\
			"A command-line utility to install, register and unregister the ExplorerBgToolRe(dux) DLL.\n"\
			"Written by LPI.\n"\
			"Use the -h option for help.\n"\
			"This project hosted at %s\n"\
			"Project ExplorerBgToolRe(dux) hosted at %s\n\n",
			_L(VER_STR_PROGRAM_NAME),
			MAJOR_VERSION,
			MINOR_VERSION,
			PATCH_VERSION,
			_L(__DATE__),
			EBTL_PROJECT_HOME,
			EXPLORERBGTOOLRE_PROJECT_HOME
			);

	// senza argomenti: stato attuale registrazione
	// -h: schermata aiuto
	// -i + [nome directory]: installazione
	// -r + <nome DLL>: registrazione
	// -u + [nome DLL]: rimozione
	// -d: reload the currently loaded DLL
	// -e: restart Explorer

	// se ha ricevuto argomenti
	if(argc > 1)
	{
		// ricava opzioni/parametri
		for(int i=0; i < argc; i++)
		{
			wcscpy_s(argument,_MAX_PATH,argv[i]);
			if(_wcsicmp(argument,L"-h")==0)
			{
				wprintf(L"usage: ebtl [option] [DLL pathname/installation directory]\n"\
						"\t-h                   this help\n"\
						"\t-i [directory]       install into default/specified directory\n"\
						"\t-r <DLL pathname>    register the DLL\n"\
						"\t-u [DLL pathname]    unregister the DLL\n"\
						"\t-d                   reload the currently loaded DLL\n"\
						"\t-e                   restart the Explorer\n"\
						"\tno option/argument   show the current registration status\n"
						"\t(note: [...] means optional, <...> means mandatory)\n\n"
						);
				goto done;
			}
			else if(_wcsicmp(argument,L"-i")==0)
				eAction = EBTL_INSTALL;
			else if(_wcsicmp(argument,L"-r")==0)
				eAction = EBTL_REGISTER;
			else if(_wcsicmp(argument,L"-u")==0)
				eAction = EBTL_UNREGISTER;
			else if(_wcsicmp(argument,L"-d")==0)
				eAction = EBTL_RELOAD_DLL;
			else if(_wcsicmp(argument,L"-e")==0)
				eAction = EBTL_RESTART_EXPLORER;
			else
			{
				if(i > 0) // deve saltare argv[0] (il nome dell'eseguibile)
				{
					if(wcsistr(argument,L".dll"))
						wcscpy_s(wzDllPath,_MAX_PATH,argv[i]);
					else
						wcscpy_s(wzInstallDir,_MAX_PATH,argv[i]);
				}
			}
		}

		// controlli

	    // verifica aver ricevuto un opzione se e' presente il pathname della DLL
		if(*wzDllPath && eAction==EBTL_STATUS)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: the pathname for the DLL has been specified with no option.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // se viene specificata una DLL, verifica che esista
		if(*wzDllPath && ::GetFileAttributesW(wzDllPath)==INVALID_FILE_ATTRIBUTES)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: DLL %s not found.\n",wzDllPath);
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica aver ricevuto il pathname della DLL per -r
		if(eAction==EBTL_REGISTER && !*wzDllPath)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: the registration process requires a valid DLL pathname.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica se con -u ha ricevuto il pathname della DLL o monnezza
		if(eAction==EBTL_UNREGISTER)
		{
			if(*wzDllPath)
				if(!wcsistr(wzDllPath,L".dll"))
				{
					_snwprintf(wzError,_countof(wzError)-1,L"Error: the unregistration process requires a valid DLL pathname.\n");
					wprintf(wzError);
					::MessageBeep(MB_ICONERROR);
					::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
					goto done;
				}
		}
	    // verifica la directory per -i
		if(eAction==EBTL_INSTALL && !*wzInstallDir)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: the installation process requires a target directory.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		if(eAction==EBTL_INSTALL && *wzDllPath)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: the installation process requires a target directory, not a DLL name.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	}
	else
		eAction = EBTL_STATUS;

	// senza argomenti solo controlla la registrazione
	if(eAction==EBTL_STATUS)
	{
		wchar_t wzDllPath[_MAX_PATH+1] = {0};
		if(explorerBgToolRe.IsRegistered(wzDllPath,_MAX_PATH))
		{
			wchar_t wzVersion[32] = {0};
			int major = 0, minor = 0, patch = 0;
			HRESULT hr = explorerBgToolRe.GetVersion(wzDllPath,major,minor,patch);
			if(SUCCEEDED(hr))
				_snwprintf(wzVersion,_countof(wzVersion)-1,L"DLL version is %d.%d.%d",major,minor,patch);
			_snwprintf(wzError,_countof(wzError)-1,L"The DLL is currently registered with %s.\n",wzDllPath);
			if(*wzVersion)
			{
				wcscat_s(wzError,_countof(wzError),wzVersion);
				wcscat_s(wzError,_countof(wzError),L"\n");
			}
		}
		else
			_snwprintf(wzError,_countof(wzError)-1,L"The DLL is NOT registered.\n");
		wprintf(wzError);
//		::MessageBeep(MB_ICONERROR);
//		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

		goto done;
	}

    // deve inizializzare COM manualmente prima di usare qualsiasi funzione Shell
	// perche' e' una app console, se fosse altrimenti sarebbe il framework a farlo
	hr = ::CoInitializeEx(NULL,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
	if(FAILED(hr))
	{
		bCoInitialized = FALSE;
		_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to initalize COM (0x%08X).\n",hr);
		wprintf(wzError);
		::MessageBeep(MB_ICONERROR);
		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
		goto done;
	}
	else
		bCoInitialized = TRUE;

//----------------------
// EBTL_RESTART_EXPLORER
//----------------------

	// se solo deve reiniziare l'Explorer
	if(eAction==EBTL_RESTART_EXPLORER)
		goto restart_explorer;

	// verifica se tiene privilegi da amministratore, richiesto per installazione, registrazione e rimozione
	if(!explorerBgToolRe.IsAdmin())
	{
		_snwprintf(wzError,_countof(wzError)-1,L"This process requires Admin privileges.\nPlease wait while trying to elevate the user permissions...");
		wprintf(wzError);
		_snwprintf(wzError,_countof(wzError)-1,L"This process requires Admin privileges.\nClick OK to to elevate the user permissions.");
		::MessageBeep(MB_ICONINFORMATION);
		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

		if(!ElevateAndRestart(argc,argv))
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to elevate user permissions.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}

		// punto cruciale: l'istanza non elevata ha finito il suo compito,
		// deve quindi chiudersi per lasciare spazio a quella nuova
		CloseConsoleWindow();

		::Sleep(1000L);

		// nel caso sopra non sia riuscito a chiudere
		goto done;
	}

//-------------
// EBTL_INSTALL
//-------------

	// installazione
	if(eAction==EBTL_INSTALL)
	{
		char szInstallDir[_MAX_PATH+1] = {0};
		char szExplorerBgToolReBin[_MAX_PATH+1] = {0};
		char szExplorerBgToolReDLL[_MAX_PATH+1] = {0};
		char szImagePath[_MAX_PATH+1] = {0};
		char szChibiPath[_MAX_PATH+1] = {0};

		// ricava/converte in ANSI la directory di installazione
		char* pszInstallDir = WideCharToAnsi(wzInstallDir,CP_UTF8);
		if(!pszInstallDir)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: memory allocation failure.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		strcpyn(szInstallDir,pszInstallDir,sizeof(szInstallDir));
		free(pszInstallDir);
		int n = (int)strlen(szInstallDir);
		if(szInstallDir[n-1]=='\\')
			szInstallDir[n-1] = '\0';
		snprintf(szExplorerBgToolReBin,sizeof(szExplorerBgToolReBin),"%s\\ExplorerBgToolRe.bin",szInstallDir);
		snprintf(szExplorerBgToolReDLL,sizeof(szExplorerBgToolReDLL),"%s\\ExplorerBgToolRe.dll",szInstallDir);
		DWORD dwError = 0L;

		// estrae la DLL (come .bin), crea le directory per le immagini ed estrae il resto dei files dalle risorse (.ini, .png, .jpg)
		BOOL bAllResExtracted = FALSE;
		BOOL bRet = ExtractDLL(szExplorerBgToolReBin);
		if(bRet)
		{
			snprintf(szImagePath,sizeof(szImagePath),"%s\\Image",szInstallDir);
			bRet = EnsurePathnameExists(szImagePath,&dwError);
		}
		if(bRet)
		{
			snprintf(szChibiPath,sizeof(szChibiPath),"%s\\Chibi",szInstallDir);
			bRet = EnsurePathnameExists(szChibiPath,&dwError);
		}
		if(bRet)
			ExtractResources(szInstallDir,bAllResExtracted);
		if(!bRet)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to extract the DLL/resources and/or create directories.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		if(!bAllResExtracted)
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Warning: unable to extract all the resources.\nSome files could be missing.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONWARNING);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
		}
	
		// se la DLL e' gia' registrata, deve prima rimuoverla
		memset(wzDllPath,_countof(wzDllPath),'\0');
		if(explorerBgToolRe.IsRegistered(wzDllPath))
		{
			wprintf(L"DLL already registered, now unregistering it...\n");
			hr = explorerBgToolRe.Unregister();
			if(FAILED(hr))
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}

		// deve riavviare l'Explorer per copiare la nuova DLL sopra la vecchia
		wprintf(L"Please wait while restarting the Explorer...\n");
		if(!explorerBgToolRe.RestartExplorer())
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: failed to restart Explorer.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}

		// ha scaricato la DLL e riavviato l'Explorer, ora deve sostituire il file esistente della DLL con quello della nuova
		char szExplorerBgToolReOldDLL[_MAX_PATH+1] = {0};
		strcpyn(szExplorerBgToolReOldDLL,szExplorerBgToolReDLL,sizeof(szExplorerBgToolReOldDLL));
		strcatn(szExplorerBgToolReOldDLL,".old",sizeof(szExplorerBgToolReOldDLL));

		// elimina il .old se gia' esiste (da un installazione precedente)
		BOOL bFlag = FileExists(szExplorerBgToolReOldDLL);
		if(bFlag)
			bFlag = ::DeleteFileA(szExplorerBgToolReOldDLL);
		else
			bFlag = TRUE;

		// il Kernel impedisce categoricamente l'eliminazione di una DLL finche' l'ultimo handle non viene chiuso, quindi
		// invece di provare ad eliminare, rinomina la DLL esistente (.dll) in .old (anche se in uso, spesso funziona)...
		if(bFlag)
			bFlag = ::MoveFileA(szExplorerBgToolReDLL,szExplorerBgToolReOldDLL);

		// ...e rinomina la DLL estratta sopra come .bin in .dll
		if(bFlag)
			bFlag = ::MoveFileA(szExplorerBgToolReBin,szExplorerBgToolReDLL);

		if(bFlag)
		{
			// tutto bene, ri-costruisce quindi il path completo per il file .dll per poterla registrare piu' sotto
			memset(wzDllPath,_countof(wzDllPath),'\0');
			_snwprintf(wzDllPath,_countof(wzDllPath)-1,L"%S",szExplorerBgToolReDLL);

			// elimina il .old

			// problema: accesso negato -> vedi sopra
//			snprintf(szExplorerBgToolReOldDLL,sizeof(szExplorerBgToolReOldDLL),"%s\\ExplorerBgToolRe.dll.old",szInstallDir);
//			::DeleteFileA(szExplorerBgToolReOldDLL);

			// soluzione: usa MoveFileEx() con il flag per l'eliminazione differita al prossimo riavvio
			snprintf(szExplorerBgToolReOldDLL,sizeof(szExplorerBgToolReOldDLL),"%s\\ExplorerBgToolRe.dll.old",szInstallDir);
			::MoveFileExA(szExplorerBgToolReOldDLL,NULL,MOVEFILE_DELAY_UNTIL_REBOOT);

			_snwprintf(wzError,_countof(wzError)-1,L"The package has been installed sucessfully.\nNext step will be the DLL registration, click OK to continue.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONINFORMATION);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
		}
		else
		{
			_snwprintf(wzError,_countof(wzError)-1,L"Error: failed to copy the new DLL, close ALL the running programs and try again.\n");
			wprintf(wzError);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}

		// dopo la installazione deve registrare la DLL
		eAction = EBTL_REGISTER;
	}

//--------------------------------
// EBTL_REGISTER / EBTL_UNREGISTER
//--------------------------------

	{
		// verifica se la DLL e' registrata o meno
		TERN tRegistered = False;
		if(*wzDllPath)
			tRegistered = explorerBgToolRe.IsRegistered(wzDllPath);
		else
			tRegistered = explorerBgToolRe.IsRegistered(wzDllPath,_countof(wzDllPath)-1);

		if(eAction==EBTL_RELOAD_DLL)
		{
			// non puo' ricaricarla se non e' gia' presente
			if(tRegistered==False)
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: there is no DLL loaded, it must be already loaded to reload it.\nUse the registration first.\n");
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// per ricaricarla, deve prima rimuoverla
			wprintf(L"Unregistering the DLL...\n");
			hr = *wzDllPath ? explorerBgToolRe.Unregister(wzDllPath) : explorerBgToolRe.Unregister();
			if(FAILED(hr))
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// la ricarica, ossia procede con la registrazione
			hr = explorerBgToolRe.Register(wzDllPath);
			if(FAILED(hr))
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to register the DLL (0x%08X).\n",hr);
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
			else
			{
				_snwprintf(wzError,_countof(wzError)-1,L"The DLL has been reloaded sucessfully.\n");
				wprintf(wzError);
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
			}
		}
		// registrazione
		else if(eAction==EBTL_REGISTER)
		{
			// se la DLL e' gia' registrata, va prima rimossa
			if(tRegistered==True)
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: the DLL il already registered, you must unregister first with the -u option.\n");
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// procede con la registrazione
			hr = explorerBgToolRe.Register(wzDllPath);
			if(FAILED(hr))
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to register the DLL (0x%08X).\n",hr);
				wprintf(wzError);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
			else
			{
				_snwprintf(wzError,_countof(wzError)-1,L"The DLL has been registered sucessfully.\n");
				wprintf(wzError);
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
			}
		}
		// rimozione
		else if(eAction==EBTL_UNREGISTER)
		{
			// procede con la rimozione
			if(tRegistered==True)
			{
				hr = *wzDllPath ? explorerBgToolRe.Unregister(wzDllPath) : explorerBgToolRe.Unregister();
				if(FAILED(hr))
				{
					_snwprintf(wzError,_countof(wzError)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
					wprintf(wzError);
					::MessageBeep(MB_ICONERROR);
					::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
					goto done;
				}
				else
				{
					_snwprintf(wzError,_countof(wzError)-1,L"The unregistration process has been completed sucessfully.\n");
					wprintf(wzError);
					::MessageBeep(MB_ICONINFORMATION);
					::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
				}
			}
			else
			{
				_snwprintf(wzError,_countof(wzError)-1,L"Warning: the DLL is NOT registered or is an orphan.\n");
				wprintf(wzError);
//				::MessageBeep(MB_ICONWARNING);
//				::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}
	}

restart_explorer:

	// se arriva qui (in cascata), deve riavviare l'Explorer per applicare i cambi
	wprintf(L"Please wait while restarting the Explorer...\n");
	if(explorerBgToolRe.RestartExplorer())
	{
		_snwprintf(wzError,_countof(wzError)-1,L"The Explorer has been restarted correctly.\n");
		wprintf(wzError);
		::MessageBeep(MB_ICONINFORMATION);
		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
	}
	else
	{
		_snwprintf(wzError,_countof(wzError)-1,L"Error: failed to restart Explorer.\n");
		wprintf(wzError);
		::MessageBeep(MB_ICONERROR);
		::MessageBoxW(NULL,wzError,L"ebtl",MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
	}

done:

	if(bCoInitialized && eAction!=EBTL_RESTART_EXPLORER)
		::CoUninitialize();

	wprintf(L"Done.\n");

    exit(0);
}
