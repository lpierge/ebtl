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
#include <tchar.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <shellapi.h>
#include <objbase.h>
#include "CExplorerBgToolRe.h"
#include "CGdiImage.h"
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
	EBTL_SET_CUSTOMFOLDER,
	EBTL_RESIZE_IMAGES,
	EBTL_RELOAD_DLL,
	EBTL_RESTART_EXPLORER,
	EBTL_STATUS
} ACTIONTYPE;

/*
	RunningFromCommandPrompt()

	Controlla se e' stato lanciato da Command Prompt o da doppio click via Explorer.
*/
BOOL RunningFromCommandPrompt(void)
{
	// l'array per contenere i PID (Process IDs), 2 elementi per capire se siamo soli o in compagnia
	DWORD processList[2] = {0};
    
	// ottiene il numero di processi attaccati alla console corrente
	DWORD processCount = ::GetConsoleProcessList(processList,2);

	// lanciato tramite doppio click da Explorer
	if(processCount <= 1)
		return(FALSE);
	// lanciato da Command Prompt o script
	else
		return(TRUE);
}

/*
	ElevateAndRestart()

	Riavvia il programma con i privilegi da amministratore.
	(la registrazione/rimozione della DLL richiede permessi da admin)
*/
BOOL ElevateAndRestart(int argc,wchar_t* argv[])
{
	wchar_t szPath[_MAX_PATH+1] = {0};
	if(::GetModuleFileNameW(NULL,szPath,_countof(szPath)-1)==0)
		return(FALSE);

	wchar_t wzDir[_MAX_PATH+1] = {0};
	::GetCurrentDirectoryW(_countof(wzDir)-1,wzDir);

	// ricostruisce la riga di comando per il nuovo processo elevato
	// ignora argv[0] (l'eseguibile) e parte da argv[1]
	wchar_t wzParams[1024] = L"";
	for(int i=1; i < argc; ++i)
	{
		// mette l'argomento tra virgolette
		wcscatn(wzParams,L"\"",_countof(wzParams));
		wcscatn(wzParams,argv[i],_countof(wzParams));
		wcscatn(wzParams,L"\" ",_countof(wzParams));
	}

	// si riesegue
	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.fMask = SEE_MASK_DEFAULT; 
	sei.lpVerb = L"runas";
	sei.lpFile = szPath;
	sei.lpParameters = wzParams; // passa gli argomenti recuperati
	sei.lpDirectory = wzDir;
	sei.nShow = SW_SHOWNORMAL;

	return(::ShellExecuteExW(&sei));
}

/*
	ExtractDLL()

	Estrae (con il nome specificato) la DLL dalle risorse.
*/
BOOL ExtractDLL(const char* lpcszOutputName,DWORD* pdwError)
{
	BOOL bRet = ExtractResource(IDR_DLL_FILE,RT_RCDATA,lpcszOutputName,pdwError);

	return(bRet);
}

/*
	ExtractResources()

	Estrae i files per la DLL dalle risorse.
*/
void ExtractResources(const char* lpcszInstallDir,BOOL& bAllResExtracted,DWORD* pdwError)
{
	DWORD dwError = 0L;

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
		wchar_t wzMessage[1024] = {0};
		::MoveFileA(szResource,szYetAnotherFile);
		_snwprintf(	wzMessage,
					_countof(wzMessage)-1,
					L"Note: the existing config.ini file has been renamed to %S to avoid being overwritten by the default config.ini file from the new installation.\n"\
					"You have to manually update the new config.ini file with the previous configuration values.\n",
					szYetAnotherFile);
		wprintf(wzMessage);
		::MessageBeep(MB_ICONWARNING);
		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
	}

	// estrae tutte le risorse
	for(int i=0; i < ARRAY_SIZE(resources_array); i++)
	{
		snprintf(szResource,sizeof(szResource),"%s\\%s",lpcszInstallDir,resources_array[i].szName);
		if(!FileExists(szResource))
			if(!ExtractResource(resources_array[i].nId,RT_RCDATA,szResource,&dwError))
			{
				*pdwError = dwError;
				bAllResExtracted = FALSE;
			}
	}
}

/*
	InstallSelfToTarget()

	Copia se stesso nella directory di installazione.
	Il programma (ebtl.exe) funziona da installatore, da configuratore e come utility in generale,
	quindi deve essere copiato (ossia copiarsi a se stesso) nella directory di installazione della
	DLL. L'utente potrebbe scaricarlo in un qualsiasi folder, dimenticarsi dove l'ha messo e andare
	poi a cercarlo, senza trovarlo, nella directory di installazione.
*/
void InstallSelfToTarget(char* szInstallDir)
{
	char szCurrentPath[_MAX_PATH+1] = {0};
	char szTargetPath[_MAX_PATH+1] = {0};

	// recupera il path dell'eseguibile attualmente in esecuzione
	if(::GetModuleFileNameA(NULL,szCurrentPath,sizeof(szCurrentPath)-1))
	{
		// costruisce il path di destinazione
		snprintf(szTargetPath,sizeof(szTargetPath),"%s\\%s.exe",szInstallDir,VER_STR_PROGRAM_NAME);

		// se il file non e' gia' nella directory di destinazione, lo copia
		// se e' gia' li' (magari di una versione precedente), la CopyFile() lo sovrascrive, a meno che non sia in uso
		if(stricmp(szCurrentPath,szTargetPath)!=0)
		{
			wchar_t wzBuffer[1024] = {0};

			if(::CopyFileA(szCurrentPath,szTargetPath,FALSE))
			{
				_snwprintf(	wzBuffer,
							_countof(wzBuffer)-1,
							L"This installer program (%S.exe) has been successfully copied to the installation folder %S for your convenience and future use.\n",
							VER_STR_PROGRAM_NAME,
							szInstallDir);
				wprintf(wzBuffer);
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzBuffer,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
			}
			else // se CopyFile() fallisce, avvisa l'utente
			{
				_snwprintf(	wzBuffer,
							_countof(wzBuffer)-1,
							L"Note: The installer (this %S.exe program) cannot be copied in the installation folder %S.\n"\
							"Once finished the installation process, do not forget to manually copy it to that folder for your convenience and future use.\n",
							VER_STR_PROGRAM_NAME,
							szInstallDir);
				wprintf(wzBuffer);
				::MessageBeep(MB_ICONWARNING);
				::MessageBoxW(NULL,wzBuffer,_L(VER_STR_PROGRAM_NAME),MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
			}
		}
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
	wchar_t wzArgument[_MAX_PATH+1] = {0};
	wchar_t argument[_MAX_PATH+1] = {0};
    CExplorerBgToolRe explorerBgToolRe;
	wchar_t wzMessage[1024] = {0};
	int nResize = -1;
	wchar_t wzResize[16] = {0};

	setlocale(LC_ALL,"");

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
	// -f + <nome directory>: imposta custom folder
	// -z + <nome directory> + <risoluzione>: resize immagini
	// -d: ricarica la DLL
	// -e: ricarica l'Explorer

	// se ha ricevuto argomenti
	if(argc > 1)
	{
		// scorre le opzioni/argomenti
		for(int i=1; i < argc; i++)
		{
			// copia il parametro presente sulla linea di comando e vede di che si tratta, se opz. o arg.
			wcsncpy(argument,argv[i],_countof(argument)-1);

			// opzione?
			if(_wcsicmp(argument,L"-h")==0)
			{
				wprintf(L"usage: ebtl [option] argument\n"\
						"\t-h                 this help\n"\
						"\t-i [dir]           install into default/specified directory\n"\
						"\t-r <DLL pathname>  register the specified DLL\n"\
						"\t-u [DLL pathname]  unregister the DLL\n"\
						"\t-f <dir|default>   set the custom folder field in config.ini to the specified directory and reload the DLL\n"\
						"\t                   use the word \"default\" (without quotes) to reset the custom folder\n"\
						"\t-z <dir>#W|H<n>    resize all the images in the specified directory to the requested width or height\n"\
						"\t                   use this example as a reference (type exactly as shown):\n"\
						"\t                   \"ebtl -z C:\\BigSizeImages#W350\"\n"\
						"\t                   this will resize all the images in C:\\BigSizeImages to a width of 350 pixels\n"\
						"\t                   (you must specify either width OR height, NOT both)\n"\
						"\t-d                 reload the (currently loaded) DLL\n"\
						"\t-e                 restart the Explorer\n"\
						"\tno option/argument show the current registration status\n\n"
						"\tnotes:\n"
						"\t\t- always leave a space between the option and the argument\n"
						"\t\t- [...] means optional, <...> means mandatory, | means OR (choose one)\n\n"
						);
				goto done;
			}
			else if(_wcsicmp(argument,L"-i")==0)
				eAction = EBTL_INSTALL;
			else if(_wcsicmp(argument,L"-r")==0)
				eAction = EBTL_REGISTER;
			else if(_wcsicmp(argument,L"-u")==0)
				eAction = EBTL_UNREGISTER;
			else if(_wcsicmp(argument,L"-f")==0)
				eAction = EBTL_SET_CUSTOMFOLDER;
			else if(_wcsicmp(argument,L"-z")==0)
				eAction = EBTL_RESIZE_IMAGES;
			else if(_wcsicmp(argument,L"-d")==0)
				eAction = EBTL_RELOAD_DLL;
			else if(_wcsicmp(argument,L"-e")==0)
				eAction = EBTL_RESTART_EXPLORER;
			else if(argument[0]=='-')
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unknown option %s, use the -h option for help.\n",argument);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
			else
			{
				// argomento, puo' essere il nome della DLL (comprensivo di path) o solo un pathname
				if(wcsistr(argument,L".dll"))
					wcsncpy(wzDllPath,argv[i],_countof(wzDllPath)-1);
				else
					wcsncpy(wzArgument,argv[i],_countof(wzArgument)-1);
			}
		}

		// controlli

	    // verifica aver ricevuto un opzione se e' presente il pathname della DLL
		if(*wzDllPath && eAction==EBTL_STATUS)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: the pathname for the DLL has been specified with no option.\nUse the -h option for help.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // se viene specificata una DLL, verifica che esista
		if(*wzDllPath && ::GetFileAttributesW(wzDllPath)==INVALID_FILE_ATTRIBUTES)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: DLL %s not found.\nUse the -h option for help.\n",wzDllPath);
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica aver ricevuto il pathname della DLL per -r
		if(eAction==EBTL_REGISTER && !*wzDllPath)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: the registration process requires a valid DLL pathname.\nUse the -h option for help.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica se con -u ha ricevuto il pathname della DLL o monnezza
		if(eAction==EBTL_UNREGISTER)
		{
			if(*wzDllPath)
				if(!wcsistr(wzDllPath,L".dll"))
				{
					_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: the unregistration process requires a valid DLL pathname.\nUse the -h option for help.\n");
					wprintf(wzMessage);
					::MessageBeep(MB_ICONERROR);
					::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
					goto done;
				}
		}
	    // verifica la directory per -i, se non e' stata passata, usa quella di default
		if(eAction==EBTL_INSTALL && !*wzArgument)
		{
			wcsncpy(wzArgument,L"C:\\ExplorerBgToolRe",_countof(wzArgument)-1);
		}
	    // verifica che per -i sia stato passato un pathname e non il nome della DLL
		if(eAction==EBTL_INSTALL && *wzDllPath)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: the installation process requires a target directory, not a DLL name.\nUse the -h option for help.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica la directory per -f
		if(eAction==EBTL_SET_CUSTOMFOLDER && !*wzArgument)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: no pathname specified for the custom folder.\nUse the -h option for help.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	    // verifica la directory ed il valore di resize per -z
		if(eAction==EBTL_RESIZE_IMAGES && !*wzArgument)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: no pathname specified for the images to be resized.\nUse the -h option for help.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		else if(eAction==EBTL_RESIZE_IMAGES && *wzArgument)
		{
			wchar_t* p = wcsrchr(wzArgument,L'#');
			if(p)
			{
				wcsncpy(wzResize,p+1,_countof(wzResize)-1);
				if(wzResize[0]==L'W' || wzResize[0]==L'H')
					nResize = (int)wcstol(wzResize+1,NULL,10);
				*p = L'\0';
			}
			if(nResize <= 0)
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: invalid resize value.\nUse the -h option for help.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}
	}
	else
		eAction = EBTL_STATUS;

//------------
// EBTL_STATUS
//------------

	// senza argomenti, visualizza la registrazione
	if(eAction==EBTL_STATUS)
	{
		// stampa a console comunque sia
		wchar_t wzRegisteredPath[_MAX_PATH+1] = {0};
		if(explorerBgToolRe.IsRegistered(wzRegisteredPath,_MAX_PATH)==True)
		{
			wchar_t wzVersion[32] = {0};
			int major=0,minor=0,patch=0;
			HRESULT hr = explorerBgToolRe.GetVersion(wzRegisteredPath,major,minor,patch);
			if(SUCCEEDED(hr))
				_snwprintf(wzVersion,_countof(wzVersion)-1,L"DLL version is %d.%d.%d",major,minor,patch);
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"The DLL is currently registered with %s.\n",wzRegisteredPath);
			if(*wzVersion)
			{
				wcscatn(wzMessage,wzVersion,_countof(wzMessage));
				wcscatn(wzMessage,L"\n",_countof(wzMessage));
			}
		}
		else
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"The DLL is NOT registered.\n");
		wprintf(wzMessage);

		// se lanciato con doppio click via Explorer, stampa con MessageBox() per far persistere il messaggio
		if(!RunningFromCommandPrompt())
		{
			wchar_t wzInfo[1024] = {0};
			_snwprintf(	wzInfo,
						_countof(wzInfo)-1,
						L"This program is the %S command line loader/utility for the ExplorerBgToolRe DLL project, hosted at:\n"\
						"%s\n\n"\
						"From the command prompt, use the -h option to show the help.\n"\
						"\n%s\nClick OK to terminate.\n",
						VER_STR_PROGRAM_NAME,
						EXPLORERBGTOOLRE_PROJECT_HOME,
						wzMessage);
			::MessageBeep(MB_ICONINFORMATION);
			::MessageBoxW(NULL,wzInfo,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
		}

		goto done;
	}

    // deve inizializzare COM manualmente prima di usare qualsiasi funzione Shell
	// perche' e' una app console, se fosse altrimenti sarebbe il framework a farlo
	hr = ::CoInitializeEx(NULL,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
	if(FAILED(hr))
	{
		bCoInitialized = FALSE;
		_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to initalize COM (0x%08X).\n",hr);
		wprintf(wzMessage);
		::MessageBeep(MB_ICONERROR);
//		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
		goto done;
	}
	else
		bCoInitialized = TRUE;

//----------------------
// EBTL_SET_CUSTOMFOLDER
//----------------------

	if(eAction==EBTL_SET_CUSTOMFOLDER)
	{
		wchar_t wzRegisteredPath[_MAX_PATH+1] = {0};
		if(explorerBgToolRe.IsRegistered(wzRegisteredPath,_MAX_PATH)==True)
		{
			wchar_t* p = wcsrchr(wzRegisteredPath,L'\\');
			if(p)
				*p = L'\0';
			
			wchar_t wzConfigFile[_MAX_PATH+1] = {0};
    		_snwprintf(wzConfigFile,_countof(wzConfigFile)-1,L"%s\\config.ini",wzRegisteredPath);

			if(wcscmp(argument,L"default")==0)
				wcscpy(wzArgument,L"");

			if(::WritePrivateProfileStringW(L"image",L"customfolder",wzArgument,wzConfigFile))
			{
				// flush della cache di sistema per forzare la scrittura su disco
				::WritePrivateProfileStringW(NULL,NULL,NULL,wzConfigFile);

				wprintf(L"Reloading the DLL...\n");
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"The pathname for the custom folder has been updated successfully.\nThe DLL will now be reloaded.\n");
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

				eAction = EBTL_RELOAD_DLL;
			}
			else
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to update the custom folder.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
//				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}
		else
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: The DLL is NOT registered.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
//			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
	}

//-------------------
// EBTL_RESIZE_IMAGES
//-------------------

	if(eAction==EBTL_RESIZE_IMAGES)
	{
		ULONG_PTR gdiToken = GdiImageHandler::Startup();
		{
		GdiImageHandler gdi;

		// elenco formati gestiti
		static const wchar_t* wzExts[] = {L"*.png",L"*.jpg",L"*.jpeg",L"*.jpe",L"*.jfif",L"*.gif"};

		// per ognuno dei formati...
		for(int i=0; i < ARRAY_SIZE(wzExts); i++)
		{
			// prepara lo skeleton di ricerca (pathname ricevuto in input + formato gestito)
			wchar_t wzSearchMask[MAX_PATH+1] = {0};
			_snwprintf(wzSearchMask,_countof(wzSearchMask)-1,L"%s\\%s",wzArgument,wzExts[i]);

			// ricerca lo skeleton			
			WIN32_FIND_DATAW fd = {0};
			HANDLE hFind = ::FindFirstFileW(wzSearchMask,&fd);
			if(hFind!=INVALID_HANDLE_VALUE)
			{
				wchar_t wzFullPath[MAX_PATH+1] = {0};

				do
				{
					// controlla che sia un file e non una directory (evita anche "." e "..")
					if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						// evita i files gia' ridimensionati
						if(wcsistr(fd.cFileName,L".resized."))
						{
							_snwprintf(wzMessage,_countof(wzMessage)-1,L"skipping %s already resized\n",fd.cFileName);
							wprintf(wzMessage);
						}
						else
						{
							// compone il percorso completo
							if(_snwprintf(wzFullPath,_countof(wzFullPath)-1,L"%s\\%s",wzArgument,fd.cFileName) > 0)
							{
								bool bResized = false;
								_snwprintf(wzMessage,_countof(wzMessage)-1,L"resizing %s...",wzFullPath);
								wprintf(wzMessage);

								if(gdi.Load(wzFullPath))
								{
									RECT rcResized = {0};
									int nOriginalWidth  = gdi.GetWidth();
									int nOriginalHeight = gdi.GetHeight();

									// ridimensionamento x width (mantenendo le proporzioni)
									if(wzResize[0]=='W' && nResize > 0)
									{
										rcResized.right  = nResize;
										rcResized.bottom = MulDiv(nOriginalHeight, nResize, nOriginalWidth);
									}
									// ridimensionamento x height (mantenendo le proporzioni)
									else if(wzResize[0]=='H' && nResize > 0)
									{
										rcResized.bottom = nResize;
										rcResized.right  = MulDiv(nOriginalWidth, nResize, nOriginalHeight);
									}

									// ridimensiona e salva con nuovo nome
									if(rcResized.right > 0 && rcResized.bottom > 0)
									{
										if(gdi.Resize(rcResized.right,rcResized.bottom))
										{
											wchar_t wzSaveAsFileName[MAX_PATH+1] = {0};
											_snwprintf(wzSaveAsFileName,_countof(wzSaveAsFileName)-1,L"%s.resized.%dx%d.jpg",wzFullPath,rcResized.right,rcResized.bottom);
											bResized = gdi.SaveAs(wzSaveAsFileName,L"image/jpeg");
										}
									}
								}

								wprintf(bResized ? L"OK\n" : L"error\n");
							}
						}
					}
				} while(::FindNextFileW(hFind,&fd));

				::FindClose(hFind);
			}
		}
		}
		GdiImageHandler::Shutdown(gdiToken);

		goto done;
	}

	// se solo deve reiniziare l'Explorer
	if(eAction==EBTL_RESTART_EXPLORER)
		goto restart_explorer;

	// verifica se tiene privilegi da amministratore, richiesto per installazione, registrazione e rimozione
	if(!explorerBgToolRe.IsAdmin())
	{
		wprintf(L"Trying to elevate user permissions...\n");
		_snwprintf(wzMessage,_countof(wzMessage)-1,L"This process requires Admin privileges.\nClick OK to to elevate the user permissions.");
		::MessageBeep(MB_ICONINFORMATION);
		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

		// non e' riuscito a switchare su admin (errore o risposta negativa nel dialogo di Windows)
		if(!ElevateAndRestart(argc,argv))
		{
			DWORD dwError = ::GetLastError();
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to elevate user permissions (%ld).\n",dwError);
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);

			CloseConsoleWindow();
			
			exit(1); // nel caso sopra non sia riuscito a chiudere
		}

		// punto cruciale: l'istanza non elevata ha finito il suo compito, deve quindi chiudersi per lasciare spazio a quella nuova
		CloseConsoleWindow();
		::Sleep(1000L);

		goto done; // nel caso sopra non sia riuscito a chiudere
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

		// se la DLL e' gia' registrata, deve prima rimuoverla e riavviare l'Explorer
		wchar_t wzRegisteredPath[_MAX_PATH+1] = {0};
		if(explorerBgToolRe.IsRegistered(wzRegisteredPath,_MAX_PATH)==True)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"The DLL is already registered in your system, therefore it will now be unregistered.");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONINFORMATION);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

			hr = explorerBgToolRe.Unregister(wzRegisteredPath);
			if(FAILED(hr))
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			wprintf(L"Please wait while restarting the Explorer...\n");
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"The Explorer will now be restarted, click OK to continue.\n");
			::MessageBeep(MB_ICONINFORMATION);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

			if(!explorerBgToolRe.RestartExplorer())
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: failed to restart Explorer.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}

		// converte in ANSI la directory di installazione perche' le funzioni di estrazione risorse etc. sono tutte ANSI
		// usa CP_ACP e non CP_UTF8 perche le funzioni 'A' native di Windows si aspettano la System Default Windows ANSI
		// Code Page (che e' esattamente cio' che rappresenta CP_ACP, come la sussidiaria Windows-1222 o 1252 per l'Europa
		// occidentale)
		char* pszInstallDir = WideCharToAnsi(wzArgument,CP_ACP);
		if(!pszInstallDir)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: memory allocation failure, unable to install the DLL.\n");
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		strcpyn(szInstallDir,pszInstallDir,sizeof(szInstallDir));
		free(pszInstallDir);
		int n = (int)strlen(szInstallDir);
		if(n > 0 && szInstallDir[n-1]=='\\')
			szInstallDir[n-1] = '\0';
		snprintf(szExplorerBgToolReBin,sizeof(szExplorerBgToolReBin),"%s\\ExplorerBgToolRe.bin",szInstallDir);
		snprintf(szExplorerBgToolReDLL,sizeof(szExplorerBgToolReDLL),"%s\\ExplorerBgToolRe.dll",szInstallDir);

		// estrae la DLL (come .bin) dalle risorse, crea le directory per le immagini ed estrae il resto dei files dalle risorse (.ini, .png, .jpg)
		wprintf(L"Extracting the package...\n");
		_snwprintf(	wzMessage,
					_countof(wzMessage)-1,
					L"The whole package (DLL in the binary form, config.ini configuration file and image samples) will now be extracted in the %S installation directory.\n",
					szInstallDir);
		::MessageBeep(MB_ICONINFORMATION);
		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

		DWORD dwError = 0L;
		BOOL bAllResExtracted = FALSE;
		BOOL bRet = ExtractDLL(szExplorerBgToolReBin,&dwError);
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
			ExtractResources(szInstallDir,bAllResExtracted,&dwError);
		if(!bRet)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to extract the DLL/resources and/or create directories (%ld).\n",dwError);
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		if(!bAllResExtracted)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Warning: unable to extract all the resources.\nSome files could be missing (%ld).\n",dwError);
			wprintf(wzMessage);
			::MessageBeep(MB_ICONWARNING);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
		}

		// ha rimosso la DLL e riavviato l'Explorer, ora deve sostituire il file esistente della vecchia DLL con quello della nuova
		// pero' non puo' semplicemente eliminare il file (vedi sotto) ma prepara quindi per poter rinominare i files...
		char szExplorerBgToolReOldDLL[_MAX_PATH+1] = {0};
		strcpyn(szExplorerBgToolReOldDLL,szExplorerBgToolReDLL,sizeof(szExplorerBgToolReOldDLL));
		strcatn(szExplorerBgToolReOldDLL,".old",sizeof(szExplorerBgToolReOldDLL));

		TERN bFlag = False;

		// invece di verificare se esiste e fare DeleteFileA(), usa direttamente MoveFileExA() con il flag MOVEFILE_REPLACE_EXISTING
		// controlla se la DLL vecchia esiste davvero, se non esiste (prima installazione) deve saltare questo passo
		if(FileExists(szExplorerBgToolReDLL))
		{		
			// rinomina .dll vecchia -> .old (e sovrascrive il vecchio .old se esiste)
			bFlag = ::MoveFileExA(szExplorerBgToolReDLL,szExplorerBgToolReOldDLL,MOVEFILE_REPLACE_EXISTING) ? True : False;
			if(bFlag==False)
			{
				dwError = ::GetLastError();
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: failed to replace %S with %S.\nSevere file lock detected (%ld). Please restart your computer to release the file lock and try the installation again.\n",szExplorerBgToolReOldDLL,szExplorerBgToolReDLL,dwError);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			}
		}
		else
		{
			// se la DLL non esiste, non c'e' nulla da spostare
			bFlag = True; 
		}

		// ...e rinomina la DLL estratta sopra come .bin in .dll (ma con logica difensiva con MoveFileExA/REPLACE)
		if(bFlag==True)
		{
			// rinomina .bin -> .dll
			bFlag = ::MoveFileExA(szExplorerBgToolReBin,szExplorerBgToolReDLL,MOVEFILE_REPLACE_EXISTING) ? True : False;
			if(bFlag==False)
			{
				bFlag = Undef;
				dwError = ::GetLastError();

				// istruisce il Kernel per eseguire la rinomina al riavvio (provaci ancora Sam!)
				::MoveFileExA(szExplorerBgToolReBin,szExplorerBgToolReDLL,MOVEFILE_DELAY_UNTIL_REBOOT);

				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: failed to rename %S to %S (%ld).\nYou need to restart your computer to get the file renamed.\nIf you see the %S once restarted, you must register it manually with the -r option (do not repeat the installation process).\n",szExplorerBgToolReBin,szExplorerBgToolReDLL,dwError,szExplorerBgToolReDLL);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			}
		}

		if(bFlag==True)
		{
			// tutto bene, ri-costruisce quindi il path completo per il file .dll per poterla registrare piu' sotto
			_snwprintf(wzDllPath,_countof(wzDllPath)-1,L"%S",szExplorerBgToolReDLL);

			// elimina il .old

			// problema: accesso negato -> vedi sopra
			//snprintf(szExplorerBgToolReOldDLL,sizeof(szExplorerBgToolReOldDLL),"%s\\ExplorerBgToolRe.dll.old",szInstallDir);
			//::DeleteFileA(szExplorerBgToolReOldDLL);

			// soluzione: usa MoveFileEx() con il flag per l'eliminazione differita al prossimo riavvio
			::MoveFileExA(szExplorerBgToolReOldDLL,NULL,MOVEFILE_DELAY_UNTIL_REBOOT);

			// si auto copia nella directory di installazione
			InstallSelfToTarget(szInstallDir);

			wprintf(L"Preparing to register the DLL...\n");
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"The package has been extracted successfully.\nThe DLL will now be registered.\n");
			::MessageBeep(MB_ICONINFORMATION);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
		}
		else if(bFlag==False)
		{
			_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: failed to copy the new DLL, close ALL the running programs and try again (%ld).\n",dwError);
			wprintf(wzMessage);
			::MessageBeep(MB_ICONERROR);
			::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
			goto done;
		}
		else if(bFlag==Undef)
			goto done;

		// dopo la installazione deve registrare la DLL
		eAction = EBTL_REGISTER;
	}

//--------------------------------------------------
// EBTL_RELOAD_DLL / EBTL_REGISTER / EBTL_UNREGISTER
//--------------------------------------------------

	{
		// verifica se la DLL e' registrata o meno
		wchar_t wzRegisteredPath[_MAX_PATH+1] = {0};
		TERN tRegistered = explorerBgToolRe.IsRegistered(wzRegisteredPath,_MAX_PATH) ? True : False;

		if(eAction==EBTL_RELOAD_DLL)
		{
			// non puo' ricaricarla se non e' gia' presente
			if(tRegistered==False)
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: there is no DLL loaded, it must be already registered to reload it.\nUse the -h option for help.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// per ricaricarla, deve prima rimuoverla
			wprintf(L"Unregistering the DLL...\n");
			hr = explorerBgToolRe.Unregister(wzRegisteredPath);
			if(FAILED(hr))
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// la ricarica, ossia procede con la registrazione
			hr = explorerBgToolRe.Register(wzRegisteredPath);
			if(FAILED(hr))
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to register the DLL (0x%08X).\n",hr);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
			else
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"The DLL has been reloaded successfully.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
			}
		}
		// registrazione
		else if(eAction==EBTL_REGISTER)
		{
			// se la DLL e' gia' registrata, va prima rimossa
			if(tRegistered==True)
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: the DLL is already registered, you must unregister it before registering again.\nUse the -h option for help.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}

			// procede con la registrazione
			hr = explorerBgToolRe.Register(wzDllPath);
			if(FAILED(hr))
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to register the DLL (0x%08X).\n",hr);
				wprintf(wzMessage);
				::MessageBeep(MB_ICONERROR);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
			else
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"The DLL has been registered successfully.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONINFORMATION);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
			}
		}
		// rimozione
		else if(eAction==EBTL_UNREGISTER)
		{
			// procede con la rimozione
			if(tRegistered==True)
			{
				hr = explorerBgToolRe.Unregister(wzRegisteredPath);
				if(FAILED(hr))
				{
					_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: unable to unregister the DLL (0x%08X).\n",hr);
					wprintf(wzMessage);
					::MessageBeep(MB_ICONERROR);
					::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
					goto done;
				}
				else
				{
					_snwprintf(wzMessage,_countof(wzMessage)-1,L"The unregistration process has been completed successfully.\n");
					wprintf(wzMessage);
					::MessageBeep(MB_ICONINFORMATION);
					::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
				}
			}
			else
			{
				_snwprintf(wzMessage,_countof(wzMessage)-1,L"Warning: the DLL is NOT registered or is an orphan.\n");
				wprintf(wzMessage);
				::MessageBeep(MB_ICONWARNING);
				::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONWARNING|MB_SYSTEMMODAL|MB_OK);
				goto done;
			}
		}
	}

//----------------------
// EBTL_RESTART_EXPLORER
//----------------------

restart_explorer:

	// se arriva qui (in cascata), deve riavviare l'Explorer (per applicare i cambi o per qualsiasi altro motivo...)
	wprintf(L"Please wait while restarting the Explorer...\n");
	_snwprintf(wzMessage,_countof(wzMessage)-1,L"The Explorer will now be restarted, click OK to continue.\n");
	::MessageBeep(MB_ICONINFORMATION);
	::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);

	if(explorerBgToolRe.RestartExplorer())
	{
		_snwprintf(wzMessage,_countof(wzMessage)-1,L"The Explorer has been restarted correctly.\n");
		wprintf(wzMessage);
		::MessageBeep(MB_ICONINFORMATION);
		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONINFORMATION|MB_SYSTEMMODAL|MB_OK);
	}
	else
	{
		_snwprintf(wzMessage,_countof(wzMessage)-1,L"Error: failed to restart Explorer.\n");
		wprintf(wzMessage);
		::MessageBeep(MB_ICONERROR);
		::MessageBoxW(NULL,wzMessage,_L(VER_STR_PROGRAM_NAME),MB_ICONERROR|MB_SYSTEMMODAL|MB_OK);
	}

done:

	// attenzione: viene omessa intenzionalmente la chiamata a CoUninitialize() perche' il riavvio di Explorer
	// lascia "appesi" i proxy COM interni della Shell, quindi chiamare CoUninitialize() causerebbe un blocco
	// del processo in console (la console rimane appesa sconcertando l'utente)
	// la pulizia viene delegata alla terminazione del processo da parte del Kernel che distrugge l'intero spazio
	// di indirizzamento del processo, chiude forzatamente tutti gli handles aperti e smantella l'Apartment COM 
	// associato al thread
	// se fosse un servizio in background (un demone che gira per mesi), omettere CoUninitialize() sarebbe un 
	// disastro, ma per un programma console, la chiusura del processo e' la pulizia definitiva
//	if(bCoInitialized)
//		::CoUninitialize();

	wprintf(L"Done.\n");

	exit(0);
}
