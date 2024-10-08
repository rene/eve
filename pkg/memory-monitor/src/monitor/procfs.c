// Copyright (c) 2024 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "config.h"
#include "monitor.h"
#include "util.h"

#include "procfs.h"

// The longest interesting line in the status file is "VmRSS: <20 digits> kB\n", which is less than 256 characters.
// So, 256 should be enough.
#define LONGEST_INTERESTING_LINE 256

static unsigned long procfs_get_rss(int pid) {
    char path[PATH_MAX + 1];
    FILE *fp;
    unsigned long rss = 0;

    // Create the path to the status file for the process
    // PID cannot be too long, so the path should not exceed PATH_MAX
    sprintf(path, "/proc/%d/status", pid);

    // Open the file
    fp = fopen(path, "r");
    if (fp == NULL) {
        syslog(LOG_INFO, "Failed to open status file for process %d\n", pid);
        return 0;
    }

    char line[LONGEST_INTERESTING_LINE];
    // If the line is longer than LONGEST_INTERESTING_LINE, it will be truncated and read in parts.
    // In any case, we will not skip the line of interest as it always fits into the buffer and starts with "VmRSS"
    // and there is a newline character at the end of the previous line.
    while (fgets(line, sizeof(line), fp) != NULL) {
        // If the line starts with "VmRSS", extract the value
        if (strncmp(line, "VmRSS", strlen("VmRSS")) == 0) {
            sscanf(line, "VmRSS: %lu kB", &rss);
            break;
        }
    }

    // Close the file
    fclose(fp);

    // Convert the value to bytes
    unsigned long rss_bytes = rss << 10;
    return rss_bytes;
}

int procfs_check_rss(int pid, unsigned long threshold) {
    unsigned long rss;
    static bool handler_executed = false;

    // Get the RSS of the process
    rss = procfs_get_rss(pid);
    if (rss == 0) {
        syslog(LOG_INFO, "Failed to get the RSS of the process %d\n", pid);
        return 1;
    }

    // Check if the RSS exceeds the threshold
    if (rss > threshold) {
        // Run the handler script only per one threshold exceed
        if (handler_executed)
            // The handler script has already been executed
            return 0;
        syslog(LOG_INFO, "----- Zedbox threshold is reached -----\n");
        char event_msg[MAX_EVENT_MSG_LENGTH];
        sprintf(event_msg, "Zedbox threshold is reached: RSS = %lu bytes (threshold = %lu bytes)\n", rss, threshold);
        // Execute the script
        handler_executed = true;
        // Run the script that is stored in the same directory as the monitor
        int status = run_handler(HANDLER_SCRIPT, event_msg);
        if (status != 0) {
            syslog(LOG_WARNING, "Failed to run the handler script\n");
        }
    } else {
        // Memory usage maybe dropped below the threshold, reset the flag
        handler_executed = false;
    }
    return 0;
}

void* procfs_monitor_thread(void *args) {
    int pid = ((monitor_procfs_args_t *) args)->pid;
    unsigned long threshold = ((monitor_procfs_args_t *) args)->threshold;
    free(args);
    // Check the RSS of the process every 10 seconds
    while (!procfs_check_rss(pid, threshold)) {
        sleep(CHECK_INTERVAL_SEC);
    }
    // We should never reach this point
    syslog(LOG_ERR, "Exiting the procfs monitor thread\n");
    return NULL;
}
