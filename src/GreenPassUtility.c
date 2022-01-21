//
//  GreenPassUtility.c
//  VanillaGreenPass
//
//  Created by Denny Caruso on 07/01/22.
//

#include "GreenPassUtility.h"

// CHECKED
void checkHealtCardNumber (char * healthCardNumber) {
    size_t healthCardNumberLength = strlen(healthCardNumber);
    if (healthCardNumberLength + 1 != HEALTH_CARD_NUMBER_LENGTH) raiseError(CHECK_HEALTH_CARD_NUMBER_SCOPE, CHECK_HEALTH_CARD_NUMBER_ERROR);
}

// CHECKED
void retrieveConfigurationData (const char * configFilePath, char ** configurationIP, unsigned short int * configurationPort) {
    FILE * filePointer;
    size_t IPlength = 0, portLength = 0;
    ssize_t getLineBytes;
    char * tempStringConfigurationIP = NULL, * stringServerAddressPort = NULL;
    
    filePointer = fopen(configFilePath, "r");
    if (!filePointer) raiseError(FOPEN_SCOPE, FOPEN_ERROR);
    if ((getLineBytes = getline((char ** restrict) & tempStringConfigurationIP, (size_t * restrict) & IPlength, (FILE * restrict) filePointer)) == -1) raiseError(GETLINE_SCOPE, GETLINE_ERROR);
    * configurationIP = (char *) calloc(strlen(tempStringConfigurationIP) - 1, sizeof(char));
    if (! *configurationIP) raiseError(CALLOC_SCOPE, CALLOC_ERROR);
    strncpy(* configurationIP, (const char *) tempStringConfigurationIP, strlen(tempStringConfigurationIP) - 1);
    checkIP(* configurationIP);
    if ((getLineBytes = getline((char ** restrict) & stringServerAddressPort, (size_t * restrict) & portLength, (FILE * restrict) filePointer)) == -1) raiseError(GETLINE_SCOPE, GETLINE_ERROR);
    * configurationPort = (unsigned short int) strtoul((const char * restrict) stringServerAddressPort, (char ** restrict) NULL, 10);
    if (configurationPort == 0 && (errno == EINVAL || errno == ERANGE)) raiseError(STRTOUL_SCOPE, STRTOUL_ERROR);
    fclose(filePointer);
}

char * getVaccineExpirationDate (void) {
    struct tm * timeInfo;
    time_t systemTime;
    time(& systemTime);
    timeInfo = localtime(& systemTime);
    timeInfo->tm_mday = 1;
    if (timeInfo->tm_mon + MONTHS_TO_WAIT_FOR_NEXT_VACCINATION + 1 > 11) {
        timeInfo->tm_year += 1;
        timeInfo->tm_mon = (timeInfo->tm_mon + MONTHS_TO_WAIT_FOR_NEXT_VACCINATION + 1) % MONTHS_IN_A_YEAR;
    } else {
        timeInfo->tm_mon = (timeInfo->tm_mon + MONTHS_TO_WAIT_FOR_NEXT_VACCINATION + 1);
    }
    
    char * vaccineExpirationDate = (char *) calloc(DATE_LENGTH, sizeof(char));
    if (!vaccineExpirationDate) raiseError(CALLOC_SCOPE, CALLOC_ERROR);
    sprintf(vaccineExpirationDate, "%02d-%02d-%d", timeInfo->tm_mday, timeInfo->tm_mon, timeInfo->tm_year + 1900);
//    vaccineExpirationDate[DATE_LENGTH - 1] = '\0';
    return vaccineExpirationDate;
}

char * getNowDate (void) {
    time_t tempTime = time(NULL);
    char * nowDate = (char *) calloc(DATE_LENGTH, sizeof(char));
    if (!nowDate) raiseError(CALLOC_SCOPE, CALLOC_ERROR);
    struct tm * timeInfo = localtime(& tempTime);
    sprintf(nowDate, "%02d-%02d-%d\n", timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);
//    nowDate[DATE_LENGTH - 1] = '\0';
    return nowDate;
}

// CHECKED
int createConnectionWithServerV (const char * configFilePath) {
    struct sockaddr_in serverV_Address;
    char * stringServerV_AddressIP = NULL;
    unsigned short int serverV_Port;
    int serverV_SocketFileDescriptor;
   
    retrieveConfigurationData(configFilePath, & stringServerV_AddressIP, & serverV_Port);
    // si imposta la comunicazione col serverV
    serverV_SocketFileDescriptor = wsocket(AF_INET, SOCK_STREAM, 0);
    memset((void *) & serverV_Address, 0, sizeof(serverV_Address));
    serverV_Address.sin_family = AF_INET;
    serverV_Address.sin_port   = htons(serverV_Port);
    if (inet_pton(AF_INET, (const char * restrict) stringServerV_AddressIP, (void *) & serverV_Address.sin_addr) <= 0) raiseError(INET_PTON_SCOPE, INET_PTON_ERROR);
    wconnect(serverV_SocketFileDescriptor, (struct sockaddr *) & serverV_Address, (socklen_t) sizeof(serverV_Address));
    free(stringServerV_AddressIP);
    return serverV_SocketFileDescriptor;
}
