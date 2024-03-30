#include<stdio.h>
#include<stdbool.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<sys/resource.h>
#include<sys/utsname.h>
#include<sys/sysinfo.h>
#include<sys/types.h>
#include<utmp.h>
#include<unistd.h>
#include<signal.h>
#include<sys/wait.h>

#define MAXSIZE 4096
#define GiB 1073741824

/* Writes 4 memory statistics (system-wide) to the FD given by writeFD.
 * Writes in the order: float physUsed, float physTot, float virtUsed, float virtTot (all in GiB)
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeMemoryDataToPipe(int writeFD) {
	// Get memory usage
	struct sysinfo systemInfo;
	float physTot, physUsed, virtTot, virtUsed;

	sysinfo(&systemInfo);
	physTot = (float)systemInfo.totalram / (float)GiB;
	physUsed = physTot - ((float)systemInfo.freeram / (float)GiB);
	virtTot = ((float)systemInfo.totalswap / (float)GiB) + physTot;
	virtUsed = ((float)systemInfo.totalswap / (float)GiB) - ((float)systemInfo.freeswap / (float)GiB) + physUsed;

	float memData[4] = {physUsed, physTot, virtUsed, virtTot};

	// Write physUsed, physTot, virtUsed, virtTot to pipe
	for (int k = 0; k < 4; k++) {
		if (write(writeFD, &memData[k], sizeof(float)) == -1) {
			perror("write");
			return -1;
		}
	}
	
	return 0;
}

/* Writes user details as a string of MAXSIZE to the FD given by writeFD, terminated by the empty string.
 * Prereq: usersFile is a valid file w/ pointer at BoF.
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeUserDataToPipe(FILE *usersFile, int writeFD) {
	struct utmp userInfo;
	char userString[MAXSIZE] = {0};	// initialize to 0
	
	// Parse through list of users and print logged in users
	while (fread(&userInfo, sizeof(struct utmp), 1, usersFile) == 1) {
		if (userInfo.ut_type != USER_PROCESS) continue;
		
		char userDetails[UT_HOSTSIZE];
		
		// Loop through userInfo.ut_line to see if it contains "pts" (strstr is unsafe on attribute "nonstring")
		bool containsPts = false;
		for (int x = 0; x < 29; x++) {
			char c = userInfo.ut_line[x];
			if (c == '\0') break;
			
			if (c == 'p' && userInfo.ut_line[x + 1] == 't' && userInfo.ut_line[x + 2] == 's') containsPts = true;
		}
		
		// TTY
		if (!containsPts) {
			strncpy(userDetails, userInfo.ut_host, 15);
			userDetails[15] = '\0';
		}
		// PTS
		else {
			// no IPV4, display tmux
			if (userInfo.ut_host[0] == '\0') {
				sprintf(userDetails, "tmux(%d).%%0", userInfo.ut_session);
			}
			// display IPV4
			else {
				strncpy(userDetails, userInfo.ut_host, 15);
				userDetails[15] = '\0';
			}
		}
		
		// Formulate full string for user details
		sprintf(userString, "%s\t%s (%s)", userInfo.ut_user, userInfo.ut_line, userDetails);
		
		// Write userString to pipe
		if (write(writeFD, userString, MAXSIZE) == -1) {
			perror("write");
			return -1;
		}
	}
	
	// Write empty string to pipe to signal termination
	if (write(writeFD, "", 1) == -1) {
		perror("write");
		return -1;
	}
	
	return 0;
}

/* Writes 2 CPU statistics to the FD given by writeFD.
 * Writes in the order: int cores, float cpuUsage (%)
 * Prereq: cpuInfo and stats are valid files w/ pointers at BoF.
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeCPUDataToPipe(FILE *cpuInfo, unsigned int delayTime, int writeFD) {
	int cores;
	long initialTotTime, newTotTime, initialTimeIdle, newTimeIdle;
	long userT = 0, niceT = 0, systemT = 0, idleT = 0, iowaitT = 0, irqT = 0, softirqT = 0;
	float cpuUsage;
	
	// Open /proc/stat
	FILE *stats = fopen("/proc/stat", "r");
	if (stats == NULL) {
		fprintf(stderr, "error: /proc/stat could not be opened\n");
		return 1;
	}
	
	// Get amount of cores
	char line[100];
	cores = -1;
	while (fgets(line, 100, cpuInfo) != NULL && cores == -1) {
		if (strstr(line, "cpu cores") != NULL) {
			sscanf(line, "%*s %*s : %d", &cores);
		}
	}
	if (cores == -1) {
		fprintf(stderr, "error: Could not find # of cpu cores\n");
		return -1;
	}
	
	// Scan stats file for system stat times
	if (fscanf(stats, "%*s %ld %ld %ld %ld %ld %ld %ld", &userT, &niceT, &systemT, &idleT, &iowaitT, &irqT, &softirqT) != 7) {
		fprintf(stderr, "warn: Some fields are missing when getting cpu stats\n");
		exit(1);
	}
	
	// Reset file pointer to beginning (also updates file)
	fseek(stats, SEEK_SET, SEEK_SET);
	
	// Set initial times
	initialTimeIdle = idleT;
	initialTotTime = userT + niceT + systemT + idleT + iowaitT + irqT + softirqT;
	
	// Sleep for provided amount of time (differs between iterations)
	sleep(delayTime);
	
	if (fscanf(stats, "%*s %ld %ld %ld %ld %ld %ld %ld", &userT, &niceT, &systemT, &idleT, &iowaitT, &irqT, &softirqT) != 7) {
		fprintf(stderr, "warn: Some fields are missing when getting cpu stats\n");
		exit(1);
	}
	
	// Set updated times delayTime seconds after initial times
	newTimeIdle = idleT;
	newTotTime = userT + niceT + systemT + idleT + iowaitT + irqT + softirqT;
	
	if (newTimeIdle - initialTimeIdle == 0 || newTotTime - initialTotTime == 0) {
		cpuUsage = 0;
	}
	else cpuUsage = 100 * (double)((newTotTime - newTimeIdle) - (initialTotTime - initialTimeIdle)) / (double)(newTotTime - initialTotTime);
	
	// Write cores and cpuUsage to pipe
	if (write(writeFD, &cores, sizeof(int)) == -1) {
		perror("write");
		return -1;
	}
	if (write(writeFD, &cpuUsage, sizeof(float)) == -1) {
		perror("write");
		return -1;
	}
	
	// Close stats file
	if (fclose(stats) != 0) {
		fprintf(stderr, "error: /proc/stat could not be closed\n");
		return 1;
	}
	
	return 0;
}
