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

#include "stats_functions.h"

/*
 * Function: extractFlagValue
 * ----------------------------
 * Extracts a positive numerical value from a string 
 * with the form: --<flagName>=<positiveNumericalValue>
 * 
 * flag: string with above flag format
 * flagName: name of flag to validate
 *
 * returns: the positive numerical value from the flag if flag is valid
 *          <0 on error (if flag is wrong format)
 */
int extractFlagValue(char *flag, char *flagName) {
	if (flag == NULL || flagName == NULL) return -1;
	
	// Ensure first two chars of flag is "--"
	char *tr = flag;
	for (int i = 0; i < 2; i++) {
		if (flag[i] != '-' || flag[i] == '\0') return -2;
		tr++;
	}
	
	// Find first occurrence of flagName and make sure it's right after the flag delimiter
	char *firstOccur;
	firstOccur = strstr(tr, flagName);
	
	// No occurrence of flagName
	if (firstOccur == NULL) return -3;
	
	// flagName not located immediately after "--"
	if (firstOccur != tr) return -4;
	
	// flagName is correct, ensure '=' located right after flagName
	firstOccur += strlen(flagName);
	if (*firstOccur != '=') return -5;
	firstOccur++;
	
	// Convert value after '=' to long int, error if anything after '=' cannot be converted
	char *leftover;
	long value = strtol(firstOccur, &leftover, 10);
	
	if (*leftover != '\0') return -6;
	
	return value;
}

/*
 * Function: printSectionLine
 * ----------------------------
 * Prints 40 '-' characters in a line to divide printed sections
 * 
 * returns: nothing
 */
void printSectionLine() {
	printf("---------------------------------------\n");
}

/*
 * Function: formatToTwoDigits
 * ----------------------------
 * Takes a number and appends a 0 to the beginning 
 * if the number is one digit
 *
 * num: Positive integer (0 <= num)
 * strAddress: Address of string to copy to, must have allocated space for at least 3 chars
 *
 * returns: the number formatted to be 2 digits long as a string
 */
void formatToTwoDigits(int num, char *strAddress) {
	if (num < 10) sprintf(strAddress, "0%d", num);
	else sprintf(strAddress, "%d", num);
}

// Struct for storing samples of memory data in linked list
typedef struct MemoryNode {
	float physUsed;	// GB
	float physTot;	// GB
	float virtUsed;	// GB
	float virtTot;	// GB
	struct MemoryNode *next;
} MemoryNode;
// Linked list operations
MemoryNode *newMNode(float physUsed, float physTot, float virtUsed, float virtTot) {
	MemoryNode *new = malloc(1 * sizeof(MemoryNode));
	if (new == NULL) fprintf(stderr, "Error allocating memory for MemoryNode\n.");
	new->physUsed = physUsed;
	new->physTot = physTot;
	new->virtUsed = virtUsed;
	new->virtTot = virtTot;
	new->next = NULL;
	return new;
}
void insertMAtTail(MemoryNode *head, MemoryNode *new) {
	MemoryNode *tr = head;
	while (tr->next != NULL) tr = tr->next;
	tr->next = new;
}
void printMList(MemoryNode *head, bool sequential, bool graphics) {
	MemoryNode *tr = head;
	MemoryNode *pre = NULL;
	float memDiff = 0;
	while (tr != NULL) {
		if (pre != NULL) {
			memDiff = tr->virtUsed - pre->virtUsed;
		}
		
		if (sequential) {
			if (tr->next == NULL) printf("%.2f GB / %.2f GB  -- %.2f GB / %.2f GB", tr->physUsed, tr->physTot, tr->virtUsed, tr->virtTot);
			// else printf("\n");
		}
		else printf("%.2f GB / %.2f GB  -- %.2f GB / %.2f GB", tr->physUsed, tr->physTot, tr->virtUsed, tr->virtTot);
		
		if ((graphics && !sequential) || (graphics && sequential && tr->next == NULL)) {
			printf("     |");
			if (tr == head) printf("o %.2f", memDiff);
			else if (memDiff == 0) printf("* %.2f", memDiff);
			else if (memDiff < 0) {
				int least = ((int)(-memDiff / 0.01) - 1 < 20) ? (int)(-memDiff / 0.01) - 1 : 20;
				for (int b = 0; b < least; b++) printf(":");
				if ((int)(-memDiff / 0.01) - 1 >= 20) printf("...");
				printf("@ %.2f", memDiff);
			}
			else {
				int least = ((int)(memDiff / 0.01) - 1 < 20) ? (int)(memDiff / 0.01) - 1 : 20;
				for (int c = 0; c < least; c++) printf("#");
				if ((int)(memDiff / 0.01) - 1 >= 20) printf("...");
				printf("* %.2f", memDiff);
			}
			printf(" (%.2f)", tr->virtUsed);
		}
		
		printf("\n");
		pre = tr;
		tr = tr->next;
	}
}
void deleteMList(MemoryNode *head) {
	MemoryNode *pre = head;
	MemoryNode *tr = NULL;
	while (pre != NULL) {
		tr = pre->next;
		free(pre);
		pre = tr;
	}
}

// Struct for storing samples of cpu usage in a linked list
typedef struct CpuUseNode {
	float cpuUse;
	struct CpuUseNode *next;
} CpuUseNode;
// Linked list operations
CpuUseNode *newCNode(float cpuUse) {
	CpuUseNode *new = malloc(1 * sizeof(CpuUseNode));
	if (new == NULL) fprintf(stderr, "Error allocating memory for CpuUseNode\n.");
	new->cpuUse = cpuUse;
	new->next = NULL;
	return new;
}
void insertCAtTail(CpuUseNode *head, CpuUseNode *new) {
	CpuUseNode *tr = head;
	while (tr->next != NULL) tr = tr->next;
	tr->next = new;
}
float getLastCpuUse(CpuUseNode *head) {
	CpuUseNode *tr = head;
	while (tr->next != NULL) tr = tr->next;
	return tr->cpuUse;
}
void printCList(CpuUseNode *head, bool sequential) {
	CpuUseNode *tr = head;
	int lineCounter = 0;
	while (tr != NULL) {
		printf("\033[K\t");
		if ((sequential && tr->next == NULL) || !sequential) {
			printf("|||");
			for (int a = 0; a < (int)(tr->cpuUse); a++) printf("|");
			printf(" %.2f\n", tr->cpuUse);
		}
		else printf("\n");
		
		lineCounter++;
		tr = tr->next;
	}
	
	printf("\033[%dA", lineCounter);
}
void deleteCList(CpuUseNode *head) {
	CpuUseNode *pre = head;
	CpuUseNode *tr = NULL;
	while (pre != NULL) {
		tr = pre->next;
		free(pre);
		pre = tr;
	}
}

int main(int argc, char **argv) {
	/* Initialize variables and CLAs */
	
	// Default program arguments
	bool system = false, user = false, graphics = false, sequential = false;
	int samples = 10, delay = 1;
	
	bool sawSamplesPosArg = false;
	bool sawAllPosArgs = false;
	bool brokePosArg = false;
	bool sawFlaggedSamples = false;
	bool sawFlaggedDelay = false;
	
	// Parse through CLAs
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--system", 8) == 0) {
			system = true;
			brokePosArg = true;
		}
		else if (strncmp(argv[i], "--user", 6) == 0) {
			user = true;
			brokePosArg = true;
		}
		else if (strncmp(argv[i], "--graphics", 10) == 0) {
			graphics = true;
			brokePosArg = true;
		}
		else if (strncmp(argv[i], "--sequential", 12) == 0) {
			sequential = true;
			brokePosArg = true;
		}
		else {
			char *leftover;
			long numArg = strtol(argv[i], &leftover, 10);
			
			// If arg is positional arg (integer)
			if (leftover[0] == '\0') {
				// More than 2 positional arguments
				if (sawAllPosArgs) {
					fprintf(stderr, "args: only a maximum of two positional arguments can be provided\n");
					return 1;
				}
				
				// Another flag separates the positional arguments
				if (brokePosArg && sawSamplesPosArg) {
					fprintf(stderr, "args: positional arguments must be contiguous\n");
					return 1;
				}
				// Second positional argument
				else if (sawSamplesPosArg) {
					if (!sawFlaggedDelay) delay = numArg;
					sawAllPosArgs = true;
				}
				// First positional argument
				else {
					if (!sawFlaggedSamples) samples = numArg;
					sawSamplesPosArg = true;
					brokePosArg = false;
				}
			}
			
			// Test if arg is --samples=N or --tdelay=T
			else {
				brokePosArg = true;
				
				int samplesRes = extractFlagValue(argv[i], "samples");
				int delayRes = extractFlagValue(argv[i], "tdelay");
				
				// No remaining possible arguments that match argv[i]
				if (samplesRes < 0 && delayRes < 0) {
					fprintf(stderr, "args: unsupported argument: \"%s\"\n", argv[i]);
					return 1;
				}
				
				if (samplesRes >= 0) {
					samples = samplesRes;
					sawFlaggedSamples = true;
				}
				else if (delayRes > 0) {
					delay = delayRes;
					sawFlaggedDelay = true;
				}
				else {
					fprintf(stderr, "args: something went wrong\n");
					return 1;
				}
			}
		}
	}
	
	if (delay == 0) {
		fprintf(stderr, "args: cannot have a delay of 0s\n");
		return 1;
	}
	
	// Declare linked lists to store system statistics
	MemoryNode *memoryHead = NULL;
	CpuUseNode *cpuHead = NULL;
	
	/* Fork thrice, pass info to children to determine their task */
	pid_t forkRet;
	
	// Initiate pipes
	int memFD[2], userFD[2], cpuFD[2];
	if (pipe(memFD) == -1 || pipe(userFD) == -1 || pipe(cpuFD) == -1) {
		perror("pipe");
		exit(1);
	}
	
	// Fork 3 children
	for (int i = 0; i < 3; i++) {
		forkRet = fork();
		
		// Child for getting MEMORY USAGE (1)
		if (forkRet == 0 && i == 0) {
			// Close read end of pipe
			if (close(memFD[0]) == -1) {
				perror("close");
			}
			
			for (int j = 0; j < samples; j++) {
				// Check writing memory data to pipe was successful
				if (writeMemoryDataToPipe(memFD[1]) == -1) exit(1);
				
				sleep(delay);
			}
			
			// Close write end of pipe
			if (close(memFD[1]) == -1) {
				perror("close");
			}
			
			exit(0);
		}
		
		// Child for getting CONNECTED USERS (2)
		else if (forkRet == 0 && i == 1) {
			// Close read end of pipe
			if (close(userFD[0]) == -1) {
				perror("close");
			}
			
			// Open /var/run/utmp in read bin mode
			FILE *usersFile;
			usersFile = fopen("/var/run/utmp", "rb");
			if (usersFile == NULL) {
				fprintf(stderr, "error: /var/run/utmp could not be opened\n");
				exit(1);
			}
			
			for (int j = 0; j < samples; j++) {
				// Reset file pointer to beginning of file (updates file as well)
				if (fseek(usersFile, SEEK_SET, SEEK_SET) != 0) {
					fprintf(stderr, "error: fseek was not successful\n");
					return -1;
				}
				
				// Check writing user data to pipe was successful
				if (writeUserDataToPipe(usersFile, userFD[1]) == -1) exit(1);
				
				sleep(delay);
			}
			
			// Close write end of pipe
			if (close(userFD[1]) == -1) {
				perror("close");
			}
			
			// Close /var/run/utmp
			if (fclose(usersFile) != 0) {
				fprintf(stderr, "error: /var/run/utmp could not be closed\n");
				exit(1);
			}
			
			exit(0);
		}
		
		// Child for getting CPU USAGE (3)
		else if (forkRet == 0 && i == 2) {
			// Close read end of pipe
			if (close(cpuFD[0]) == -1) {
				perror("close");
			}
			
			// Open /proc/cpuinfo for CPU info
			FILE *cpuInfo;
			cpuInfo = fopen("/proc/cpuinfo", "r");
			if (cpuInfo == NULL) {
				fprintf(stderr, "error: /proc/cpuinfo could not be opened\n");
				exit(1);
			}
			
			for (int j = 0; j < samples; j++) {
				// Reset file pointer to beginning
				fseek(cpuInfo, SEEK_SET, SEEK_SET);
				
				int cpuSampleDelay;
				
				// Only sample for 1s on first iteration
				if (j == 0) cpuSampleDelay = 1;
				// Else sample for (delay) seconds
				else cpuSampleDelay = delay;
				
				// Check writing user data to pipe was successful
				if (writeCPUDataToPipe(cpuInfo, cpuSampleDelay, cpuFD[1]) == -1) exit(1);
				
				// No delay necessary, as writeCPUDataToPipe already delays for appropriate time
			}
			
			// Close write end of pipe
			if (close(cpuFD[1]) == -1) {
				perror("close");
			}
			
			// Close /proc/cpuinfo
			if (fclose(cpuInfo) != 0) {
				fprintf(stderr, "error: /proc/cpuinfo could not be closed\n");
				exit(1);
			}
			
			exit(0);
		}
		
		else if (forkRet < 0) {
			perror("fork");
			exit(1);
		}
	}
	
	/* PARENT PROCESS */
	
	// Close write end of pipes
	if (close(memFD[1]) == -1 || close(userFD[1]) == -1 || close(cpuFD[1]) == -1) {
		perror("close");
	}
	
	// Initial variables before entering loop
	int cores = -1;
	
	for (int i = 0; i < samples; i++) {
		// Read data from child handling memory usage
		float memData[4];
		for (int i = 0; i < 4; i++) {
			if (read(memFD[0], &memData[i], sizeof(float)) <= 0) {
				fprintf(stderr, "Could not read 4 elements from memory pipe\n");
				exit(1);
			}
		}
		
		// Add to memory linked list
		if (memoryHead == NULL) memoryHead = newMNode(memData[0], memData[1], memData[2], memData[3]);
		else {
			MemoryNode *new = newMNode(memData[0], memData[1], memData[2], memData[3]);
			insertMAtTail(memoryHead, new);
		}
		
		// Get self-memory utilization
		struct rusage usage;
		getrusage(RUSAGE_SELF, &usage);
		long curProgMem = usage.ru_maxrss;
		
		// Clear terminal and move cursor to top left
		if (!sequential) printf("\033[2J\033[H");
		
		// Print initial data (samples, delay, self-memory utilization)
		if (sequential) printf(">>> iteration %d\n", i);
		else printf("Nbr of samples: %d -- every %d secs\n", samples, delay);
		printf(" Memory usage: %ld kilobytes\n", curProgMem);
		printSectionLine();
		
		/* Print memory usage (system-wide) */
		if (!user || system) {
			printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
			printMList(memoryHead, sequential, graphics);
			for (int j = 0; j < samples - i - 1; j++) printf("\n");		// fill blank space for memory section
			printSectionLine();
		}
		
		/* Print connected users */
		if (user || !system) printf("### Sessions/users ###\n");
		
		// Read from child handling connected users
		char userLine[MAXSIZE];
		while (read(userFD[0], userLine, MAXSIZE) > 0) {
			if (userLine[0] == '\0') break;	// stop reading from pipe when we see empty string
			
			if (user || !system) printf(" %s\n", userLine);
		}
		
		if (user || !system) printSectionLine();
		
		/* Print CPU usage */
		float cpuUsage;
		
		if (!user || system) {
			if (cores == -1) printf("Number of cores: \n");		// num cores not initialized by CPU child process yet
			else printf("Number of cores: %d\n", cores);
		}
		
		if (cpuHead != NULL && (!user || system)) {
			printf(" total cpu use = %.2f%%\n", getLastCpuUse(cpuHead));
			if (graphics) printCList(cpuHead, sequential);
		}
		
		// Read from child handling CPU usage
		if (read(cpuFD[0], &cores, sizeof(int)) <= 0) {
			fprintf(stderr, "Could not read # of cores from pipe\n");
			exit(1);
		}
		if (read(cpuFD[0], &cpuUsage, sizeof(float)) <= 0) {
			fprintf(stderr, "Could not read cpuUsage from pipe\n");
			exit(1);
		}
		
		// Write new cpu usage stat to linked list
		if (cpuHead == NULL) cpuHead = newCNode(cpuUsage);
		else {
			CpuUseNode *new = newCNode(cpuUsage);
			insertCAtTail(cpuHead, new);
		}
		
		// Update (print) cpu usage without clearing screen
		if (!user || system) {
			if (i > 0) printf("\033[1A\033[K");	// to refresh cpu usage without clearing entire screen
			printf("\033[1A\033[K");
			printf("Number of cores: %d\n", cores);
			printf(" total cpu use = %.2f%%\n", getLastCpuUse(cpuHead));
			if (graphics) printCList(cpuHead, sequential);
		}
	}
	
	// Sleep remaining time
	sleep(delay - 1);
	
	// Close read end of pipes
	if (close(memFD[0]) == -1 || close(userFD[0]) == -1 || close(cpuFD[0]) == -1) {
		perror("close");
	}
	
	if (graphics) printf("\033[%dB", samples);	// Realign output pointer
	
	/* GET SYSTEM INFORMATION */
	struct utsname kernalInfo;
	uname(&kernalInfo);
	char *systemName = kernalInfo.sysname;
	char *machineName = kernalInfo.nodename;
	char *version = kernalInfo.version;
	char *release = kernalInfo.release;
	char *architecture = kernalInfo.machine;
	
	// Create time string
	struct sysinfo systemInfo;
	sysinfo(&systemInfo);
	long uptime = systemInfo.uptime;
	
	// TEST
	uptime += 200000;
	int daysUptime = floor(uptime / 86400);
	int hoursUptime = floor((uptime % 86400) / 3600);
	int minsUptime = floor((uptime % 3600) / 60);
	int secsUptime = uptime % 60;
	char totalHoursUpStr[256];
	char hoursUpStr[3];
	char minsUpStr[3];
	char secsUpStr[3];
	formatToTwoDigits(floor(uptime / 3600), totalHoursUpStr);
	formatToTwoDigits(hoursUptime, hoursUpStr);
	formatToTwoDigits(minsUptime, minsUpStr);
	formatToTwoDigits(secsUptime, secsUpStr);
	
	// Print system information
	printSectionLine();
	printf("### System Information ###\n");
	printf(" System Name = %s\n", systemName);
	printf(" Machine Name = %s\n", machineName);
	printf(" Version = %s\n", version);
	printf(" Release = %s\n", release);
	printf(" Architecture = %s\n", architecture);
	printf(" System running since last reboot: %d days %s:%s:%s (%s:%s:%s)\n", daysUptime, hoursUpStr, minsUpStr, secsUpStr, totalHoursUpStr, minsUpStr, secsUpStr);
	printSectionLine();
	
	// Free allocated memory
	deleteMList(memoryHead);
	deleteCList(cpuHead);
	
	return 0;
}
