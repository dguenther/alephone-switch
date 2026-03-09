#include "shell_options.h"
#include "shell.h"
#include "csstrings.h"
#include "Logging.h"
#include "alephversion.h"
#include <SDL2/SDL_main.h>

#ifdef __SWITCH__
#include "switch_network.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" {
#include <switch/result.h>
#include <switch/services/nifm.h>
#include <switch/services/ssl.h>
#include <switch/runtime/devices/socket.h>
#include <switch/runtime/nxlink.h>
}

static int s_nxlinkSock = -1;

static void initNxLink()
{
    if (!switch_network_ready())
        return;

    s_nxlinkSock = nxlinkStdio();
}

static void deinitNxLink()
{
    if (s_nxlinkSock >= 0)
    {
        close(s_nxlinkSock);
        s_nxlinkSock = -1;
    }
}

extern "C" void userAppInit()
{
    switch_network_init();
    initNxLink();
}

extern "C" void userAppExit()
{
    deinitNxLink();
    switch_network_shutdown();
}

alignas(16) __attribute__((used)) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

extern "C" __attribute__((used)) void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
    int i;
    FILE *f = fopen("sdmc:/switch/alephone/exception_dump.txt", "w");
    if(f==NULL)return;

    fprintf(f, "error_desc: 0x%x\n", ctx->error_desc);//You can also parse this with ThreadExceptionDesc.
    //This assumes AArch64, however you can also use threadExceptionIsAArch64().
    for(i=0; i<29; i++)fprintf(f, "[X%d]: 0x%lx\n", i, ctx->cpu_gprs[i].x);
    fprintf(f, "fp: 0x%lx\n", ctx->fp.x);
    fprintf(f, "lr: 0x%lx\n", ctx->lr.x);
    fprintf(f, "sp: 0x%lx\n", ctx->sp.x);
    fprintf(f, "pc: 0x%lx\n", ctx->pc.x);

    //You could print fpu_gprs if you want.

    fprintf(f, "pstate: 0x%x\n", ctx->pstate);
    fprintf(f, "afsr0: 0x%x\n", ctx->afsr0);
    fprintf(f, "afsr1: 0x%x\n", ctx->afsr1);
    fprintf(f, "esr: 0x%x\n", ctx->esr);

    fprintf(f, "far: 0x%lx\n", ctx->far.x);

    fclose(f);
}

#endif

int main(int argc, char** argv)
{
	// Print banner (don't bother if this doesn't appear when started from a GUI)
	char app_name_version[256];
	expand_app_variables(app_name_version, "Aleph One $appLongVersion$");
	printf("%s\n%s\n\n"
		"Original code by Bungie Software <http://www.bungie.com/>\n"
		"Additional work by Loren Petrich, Chris Pruett, Rhys Hill et al.\n"
		"TCP/IP networking by Woody Zenfell\n"
		"SDL port by Christian Bauer <Christian.Bauer@uni-mainz.de>\n"
#if defined(__MACH__) && defined(__APPLE__)
		"Mac OS X/SDL version by Chris Lovell, Alexander Strange, and Woody Zenfell\n"
#endif
		"\nThis is free software with ABSOLUTELY NO WARRANTY.\n"
		"You are welcome to redistribute it under certain conditions.\n"
		"For details, see the file COPYING.\n"
#if defined(__WIN32__)
		// Windows is statically linked against SDL, so we have to include this:
		"\nSimple DirectMedia Layer (SDL) Library included under the terms of the\n"
		"GNU Library General Public License.\n"
		"For details, see the file COPYING.SDL.\n"
#endif
#if !defined(DISABLE_NETWORKING)
		"\nBuilt with network play enabled.\n"
#endif
		, app_name_version, A1_HOMEPAGE_URL
	);

	shell_options.parse(argc, argv);

	auto code = 0;

	try {

		// Initialize everything
		initialize_application();

		for (std::vector<std::string>::iterator it = shell_options.files.begin(); it != shell_options.files.end(); ++it)
		{
			if (handle_open_document(*it))
			{
				break;
			}
		}

		// Run the main loop
		main_event_loop();

	}
	catch (std::exception& e) {
		try
		{
			logFatal("Unhandled exception: %s", e.what());
		}
		catch (...)
		{
		}
		code = 1;
	}
	catch (...) {
		try
		{
			logFatal("Unknown exception");
		}
		catch (...)
		{
		}
		code = 1;
	}

	try
	{
		shutdown_application();
	}
	catch (...)
	{

	}

	return code;
}
