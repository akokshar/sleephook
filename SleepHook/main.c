//
//  main.c
//  SleepHook
//
//  Created by Alexander Koksharov on 29/9/12.
//  Copyright (c) 2012 Alexandr Koksharov. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

void usage();
static void callbackSignal(int signalCode);
static void callbackPower(void *refcon, io_service_t service, uint32_t messageType, void *messageArgument);
void registerForNotifications();
void deRegisterForNotifications();
void executeScript(const char *script);

static struct {
    char *sleepHookScript;
    char *wakeHookScript;
    
    io_connect_t pmRoot;
    io_object_t pmNotifier;
    IONotificationPortRef pmNotificationPortRef;
} selfContext;


int main(int argc, char * const * argv) {
    
    memset(&selfContext, 0, sizeof(selfContext));
    
    int ch;
    while ((ch = getopt(argc, argv, "w:s:")) != -1) {
        switch (ch) {
            case 's':
                selfContext.sleepHookScript = optarg;
                break;
                
            case 'w':
                selfContext.wakeHookScript = optarg;
                break;
                
            default:
                usage();
                exit(EXIT_FAILURE);
                break;
        }
    }
    
    if (!selfContext.sleepHookScript && !selfContext.wakeHookScript) {
        usage();
        exit(EXIT_FAILURE);
    }
        
	signal(SIGINT,  callbackSignal);
	signal(SIGQUIT, callbackSignal);
	signal(SIGABRT, callbackSignal);
	signal(SIGKILL, callbackSignal);
	signal(SIGTERM, callbackSignal);

    fclose (stdin);
    fclose (stdout);
    fclose (stderr);
    
    registerForNotifications();
    CFRunLoopRun();
    
    syslog(LOG_ERR, "Unexpected termination\n");
    deRegisterForNotifications();
    exit(EXIT_SUCCESS);
}

void usage() {
    printf("Usage: SleepHook { [-s pathname] [-w pathname] }\n");
    printf("   -s: script to be executed on 'WillSleep' event\n");
    printf("   -w: script to be executed on 'WillPowerOn' event\n");
}

static void callbackSignal(int signalCode) {
	switch (signalCode) {
        case SIGINT:
        case SIGQUIT:
        case SIGABRT:
        case SIGKILL:
        case SIGTERM:
            syslog(LOG_DEBUG, "Terminating signal received (%i)\n", signalCode);
            
            deRegisterForNotifications();
            
            exit(EXIT_SUCCESS);
            break;
	}
}

void executeScript(const char *script) {
    if (access(script, X_OK) == 0) {
        syslog(LOG_DEBUG, "Execution of script '%s' finished with '%i'. \n", script, system(script));
    } else {
        syslog(LOG_ERR, "'%s' could not be executed. (%m)\n",script);
    }

}

void registerForNotifications() {
	selfContext.pmRoot = IORegisterForSystemPower(
                                                  NULL,
                                                  &selfContext.pmNotificationPortRef,
                                                  callbackPower,
                                                  &selfContext.pmNotifier
                                                  );
    
	if (selfContext.pmRoot == MACH_PORT_NULL) {
		syslog(LOG_ERR, "Registering for power notifications failed. (%m)\n");
		exit(EXIT_FAILURE);
	}
    
    CFRunLoopAddSource(
                       CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(selfContext.pmNotificationPortRef),
                       kCFRunLoopCommonModes
                       );
    
    syslog(LOG_DEBUG, "Registered for notification\n");
}

void deRegisterForNotifications() {
    
    CFRunLoopRemoveSource(
                          CFRunLoopGetCurrent(),
                          IONotificationPortGetRunLoopSource(selfContext.pmNotificationPortRef),
                          kCFRunLoopCommonModes
                          );
    
    IODeregisterForSystemPower(&selfContext.pmNotifier);
    IOServiceClose(selfContext.pmRoot);
    IONotificationPortDestroy(selfContext.pmNotificationPortRef);

    syslog(LOG_DEBUG, "Deregistered for notification\n");
}

static void callbackPower(void *refcon, io_service_t service, uint32_t messageType, void *messageArgument ) {
    
    switch ( messageType ) {
            
        case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
             Applications have a chance to prevent sleep by calling IOCancelPowerChange.
             Most applications should not prevent idle sleep.
             
             Power Management waits up to 30 seconds for you to either allow or deny idle
             sleep. If you don't acknowledge this power change by calling either
             IOAllowPowerChange or IOCancelPowerChange, the system will wait 30
             seconds then go to sleep.
             */
            
            //Uncomment to cancel idle sleep
            //IOCancelPowerChange( root_port, (long)messageArgument );
            // we will allow idle sleep
            
            syslog(LOG_DEBUG, "kIOMessageCanSystemSleep\n");
            
            IOAllowPowerChange(
                               selfContext.pmRoot,
                               (long)messageArgument
                               );
            break;
            
        case kIOMessageSystemWillSleep:
            /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
             IOCancelPowerChange to acknowledge this message, sleep will be
             delayed by 30 seconds.
             
             NOTE: If you call IOCancelPowerChange to deny sleep it returns
             kIOReturnSuccess, however the system WILL still go to sleep.
             */
            
            syslog(LOG_DEBUG, "kIOMessageSystemWillSleep\n");
            executeScript(selfContext.sleepHookScript);
            
            IOAllowPowerChange(
                               selfContext.pmRoot,
                               (long)messageArgument
                               );
            break;
            
        case kIOMessageSystemWillPowerOn:
            //System has started the wake up process...
            syslog(LOG_DEBUG, "kIOMessageSystemWillPowerOn\n");
            break;
            
        case kIOMessageSystemHasPoweredOn:
            //System has finished waking up...
            syslog(LOG_DEBUG, "kIOMessageSystemHasPoweredOn\n");
            executeScript(selfContext.wakeHookScript);
            break;
            
        default:
            break;
            
    }
    
}



