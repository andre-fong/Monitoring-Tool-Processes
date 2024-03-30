#include<stdio.h>
#include<stdlib.h>

#define MAXSIZE 4096
#define GiB 1073741824

#ifndef __Graph_Algos_header
#define __Graph_Algos_header

/* Writes 4 memory statistics (system-wide) to the FD given by writeFD.
 * Writes in the order: float physUsed, float physTot, float virtUsed, float virtTot (all in GiB)
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeMemoryDataToPipe(int writeFD);

/* Writes user details as a string of MAXSIZE to the FD given by writeFD, terminated by the empty string.
 * Prereq: usersFile is a valid file w/ pointer at BoF.
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeUserDataToPipe(FILE *usersFile, int writeFD);

/* Writes 2 CPU statistics to the FD given by writeFD.
 * Writes in the order: int cores, float cpuUsage (%)
 * Prereq: cpuInfo and stats are valid files w/ pointers at BoF.
 * Returns: 0 on success, 
 *          -1 on error
 */
int writeCPUDataToPipe(FILE *cpuInfo, unsigned int delayTime, int writeFD);

#endif
