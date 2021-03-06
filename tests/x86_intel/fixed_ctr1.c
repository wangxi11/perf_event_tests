/* This tests using the intel Fixed Counter 1 */
/* Which maps to UNHALTED_CORE_CYCLES         */
/* Support added in Linux 2.6.X */
/* Note: bugs where count not same as same counter in general purpose counter */

/* You cannot specify that you want the fixed counter, Linux */
/* schedules it if available.				     */
/* The NMI watchdog often grabs the fixed counter, making the test */
/* not notice the issue */

/* by Vince Weaver, vincent.weaver@maine.edu          */

static char test_string[]="Testing fixed counter 1 event...";
static int quiet=0;

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>

#include "perf_event.h"
#include "test_utils.h"
#include "perf_helpers.h"
#include "instructions_testcode.h"

#define MAX_OPEN	16

int main(int argc, char **argv) {

	struct perf_event_attr pe;
	int result=0;
	int fd[MAX_OPEN];
	int i,j;
	int num_counters=0;
	int errors=0,total_errors=0;
	int watchdog_enabled;
	int warn=0;

	long long values[256];

	quiet=test_quiet();
	watchdog_enabled=detect_nmi_watchdog();

	if (!quiet) {
		printf("This test checks the intel fixed counter 1\n");
		printf("This is a best effort, Linux does not let you\n");
		printf("specify which counter events are scheduled in.\n");
		printf("Anyway, all the values should match.\n\n");
	}

	if ((!quiet) && (watchdog_enabled)) {
		printf("The NMI watchdog is enabled.\n");
		printf("This often grabs fixed counter 1,\n");
		printf("hiding the issue.\n\n");
		warn=1;
	}

	if (detect_vendor()!=VENDOR_INTEL) test_skip(test_string);

	memset(&pe,0,sizeof(struct perf_event_attr));

	pe.type=PERF_TYPE_HARDWARE;
	pe.size=sizeof(struct perf_event_attr);
	pe.config=PERF_COUNT_HW_CPU_CYCLES;
        pe.read_format=PERF_FORMAT_GROUP;

        pe.exclude_kernel=1;
        pe.exclude_hv=1;

	for(j=0;j<20;j++) {
		errors=0;
		/* Detect max events we can have open */
		for(i=0;i<MAX_OPEN;i++) {
			pe.disabled=(i==0);
			pe.pinned=(i==0);
			fd[i]=perf_event_open(&pe,0,-1,i==0?-1:fd[0],0);
			if (fd[i]<0) {
//				if (!quiet) fprintf(stderr,"Stopped at event#%d: %s\n",i,strerror(errno));
				break;
			}
		}

		num_counters=i;
		if (num_counters<1) {
			test_fail(test_string);
		}

		/* Due to foolishness in the perf_event interface */
		/* The number of counters cannot be properly detected */
		/* At open time of the watchdog counter is enabled */
		if (watchdog_enabled) {
			for(i=0;i<num_counters;i++) {
				close(fd[i]);
			}
			num_counters--;
			for(i=0;i<num_counters;i++) {
				pe.disabled=(i==0);
				pe.pinned=(i==0);
				fd[i]=perf_event_open(&pe,0,-1,i==0?-1:fd[0],0);
			}

		}

		ioctl(fd[0],PERF_EVENT_IOC_ENABLE,0);

		result+=instructions_million();

		ioctl(fd[0],PERF_EVENT_IOC_DISABLE,0);

		read(fd[0],values,128);

		for(i=0;i<num_counters;i++) {
			if (!quiet) {
				printf("%d %lld, ",i,values[i+1]);
			}
			if (values[i+1]!=values[1]) {
				errors++;
			}
		}
		if (!quiet) printf("\n");

		for(i=0;i<num_counters;i++) {
			close(fd[i]);
		}

		if (errors) {
			if (!quiet) {
				fprintf(stderr,"Some results don't match!\n");
			}
			total_errors++;
		}
	}

	if (total_errors>0) {
		test_fail(test_string);
	}

	if (warn) test_warn(test_string);

	test_pass(test_string);

	return 0;
}
