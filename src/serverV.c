//
//  serverV.c
//  VanillaGreenPass
//
//  Created by Denny Caruso on 08/01/22.
//

#include "serverV.h"

int main (int argc, char * argv[]) {
    int listenFileDescriptor, connectionFileDescriptor, enable = TRUE, threadCreationReturnValue;
    pthread_t singleTID;
    pthread_attr_t attr;
    unsigned short int serverV_Port, requestIdentifier;
    struct sockaddr_in serverV_Address, client;
    
    checkUsage(argc, (const char **) argv, SERVER_V_ARGS_NO, expectedUsageMessage);
    serverV_Port = (unsigned short int) strtoul((const char * restrict) argv[1], (char ** restrict) NULL, 10);
    if (serverV_Port == 0 && (errno == EINVAL || errno == ERANGE)) raiseError(STRTOUL_SCOPE, STRTOUL_ERROR);
    
    listenFileDescriptor = wsocket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(listenFileDescriptor, SOL_SOCKET, SO_REUSEADDR, & enable, (socklen_t) sizeof(enable))  == -1) raiseError(SET_SOCK_OPT_SCOPE, SET_SOCK_OPT_ERROR);
    memset((void *) & serverV_Address, 0, sizeof(serverV_Address));
    memset((void *) & client, 0, sizeof(client));
    serverV_Address.sin_family      = AF_INET;
    serverV_Address.sin_addr.s_addr = htonl(INADDR_ANY);
    serverV_Address.sin_port        = htons(serverV_Port);
    wbind(listenFileDescriptor, (struct sockaddr *) & serverV_Address, (socklen_t) sizeof(serverV_Address));
    wlisten(listenFileDescriptor, LISTEN_QUEUE_SIZE * LISTEN_QUEUE_SIZE);
    
    if (pthread_mutex_init(& fileSystemAccessMutex, (const pthread_mutexattr_t *) NULL) != 0) raiseError(PTHREAD_MUTEX_INIT_SCOPE, PTHREAD_MUTEX_INIT_ERROR);
    if (pthread_mutex_init(& connectionFileDescriptorMutex, (const pthread_mutexattr_t *) NULL) != 0) raiseError(PTHREAD_MUTEX_INIT_SCOPE, PTHREAD_MUTEX_INIT_ERROR);
    if (pthread_attr_init(& attr) != 0) raiseError(PTHREAD_MUTEX_ATTR_INIT_SCOPE, PTHREAD_MUTEX_ATTR_INIT_ERROR);
    if (pthread_attr_setdetachstate(& attr, PTHREAD_CREATE_DETACHED) != 0) raiseError(PTHREAD_ATTR_DETACH_STATE_SCOPE, PTHREAD_ATTR_DETACH_STATE_ERROR);
    
    while (TRUE) {
        ssize_t fullReadReturnValue;
        socklen_t clientAddressLength = (socklen_t) sizeof(client);
        connectionFileDescriptor = waccept(listenFileDescriptor, (struct sockaddr *) & client, (socklen_t *) & clientAddressLength);
        if ((fullReadReturnValue = fullRead(connectionFileDescriptor, (void *) & requestIdentifier, sizeof(requestIdentifier))) != 0) raiseError(FULL_READ_SCOPE, (int) fullReadReturnValue);
        
        if (pthread_mutex_lock(& connectionFileDescriptorMutex) != 0) raiseError(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR);
        int * threadConnectionFileDescriptor = (int *) calloc(1, sizeof(* threadConnectionFileDescriptor));
        if (!threadConnectionFileDescriptor) raiseError(CALLOC_SCOPE, CALLOC_ERROR);
        if ((* threadConnectionFileDescriptor = dup(connectionFileDescriptor)) < 0) raiseError(DUP_SCOPE, DUP_ERROR);
        if (pthread_mutex_unlock(& connectionFileDescriptorMutex) != 0)  raiseError(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR);
        switch (requestIdentifier) {
            case centroVaccinaleSender:
                // passo sempre lo stesso TID. Non mi interessa fare join. I thread sono detached.
                if ((threadCreationReturnValue = pthread_create(& singleTID, & attr, & centroVaccinaleRequestHandler, threadConnectionFileDescriptor)) != 0) raiseError(PTHREAD_CREATE_SCOPE, PTHREAD_CREATE_ERROR);
                break;
            case clientS_viaServerG_Sender:
                if ((threadCreationReturnValue = pthread_create(& singleTID, & attr, & clientS_viaServerG_RequestHandler, threadConnectionFileDescriptor)) != 0) raiseError(PTHREAD_CREATE_SCOPE, PTHREAD_CREATE_ERROR);
                break;
            case clientT_viaServerG_Sender:
                if ((threadCreationReturnValue = pthread_create(& singleTID, & attr, & clientT_viaServerG_RequestHandler, threadConnectionFileDescriptor)) != 0) raiseError(PTHREAD_CREATE_SCOPE, PTHREAD_CREATE_ERROR);
                break;
            default:
                raiseError(INVALID_SENDER_ID_SCOPE, INVALID_SENDER_ID_ERROR);
                break;
        }
        wclose(connectionFileDescriptor);
    }
    // codice mai eseguito
    if (pthread_attr_destroy(& attr) != 0) raiseError(PTHREAD_MUTEX_ATTR_DESTROY_SCOPE, PTHREAD_MUTEX_ATTR_DESTROY_ERROR);
    wclose(listenFileDescriptor);
    exit(0);
}

void * centroVaccinaleRequestHandler (void * args) {
    if (pthread_mutex_lock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR);
    int threadConnectionFileDescriptor = * ((int *) args);
    if (pthread_mutex_unlock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR);
    ssize_t fullWriteReturnValue, fullReadReturnValue, getLineBytes;
    size_t effectiveLineLength = 0;
    char * singleLine = NULL, * nowDateString = NULL;
    char dateCopiedFromFile[DATE_LENGTH];
    struct tm firstTime, secondTime;
    time_t scheduledVaccinationDate, requestVaccinationDate = 0;
    double elapsedMonths;
    enum boolean isVaccineBlocked = FALSE, healthCardNumberWasFound = FALSE;
    FILE * originalFilePointer, * tempFilePointer;
    
    centroVaccinaleRequestToServerV * newCentroVaccinaleRequest = (centroVaccinaleRequestToServerV *) calloc(1, sizeof(* newCentroVaccinaleRequest));
    if (!newCentroVaccinaleRequest) threadAbort(CALLOC_SCOPE, CALLOC_ERROR, threadConnectionFileDescriptor, args);
    
    serverV_ReplyToCentroVaccinale * newServerV_Reply = (serverV_ReplyToCentroVaccinale *) calloc(1, sizeof(* newServerV_Reply));
    if (!newServerV_Reply) threadAbort(CALLOC_SCOPE, CALLOC_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest);
    
    if ((fullReadReturnValue = fullRead(threadConnectionFileDescriptor, (void *) newCentroVaccinaleRequest, sizeof(* newCentroVaccinaleRequest))) != 0) threadAbort(FULL_READ_SCOPE, (int) fullReadReturnValue, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
    strncpy((char *) newServerV_Reply->healthCardNumber, (const char *) newCentroVaccinaleRequest->healthCardNumber, HEALTH_CARD_NUMBER_LENGTH);
    newServerV_Reply->requestResult = FALSE;
    
    if (pthread_mutex_lock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
    originalFilePointer = fopen(dataPath, "r");
    tempFilePointer = fopen(tempDataPath, "w");
    if (!originalFilePointer || !tempFilePointer) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) {} threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
        threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
    }
    
    // controllo 5 mesi non ancora passati
    while ((getLineBytes = getline((char ** restrict) & singleLine, (size_t * restrict) & effectiveLineLength, (FILE * restrict) originalFilePointer)) != -1) {
        if ((strncmp((const char *) newServerV_Reply->healthCardNumber, (const char *) singleLine, HEALTH_CARD_NUMBER_LENGTH - 1)) == 0) {
            healthCardNumberWasFound = TRUE;
            strncpy((char *) dateCopiedFromFile, (const char *) singleLine + HEALTH_CARD_NUMBER_LENGTH, DATE_LENGTH - 1);
            dateCopiedFromFile[DATE_LENGTH - 1] = '\0';
            memset(& firstTime, 0, sizeof(firstTime));
            memset(& secondTime, 0, sizeof(secondTime));
            
            firstTime.tm_mday = (int) strtol((const char * restrict) & dateCopiedFromFile[0], (char ** restrict) NULL, 10);
            firstTime.tm_mon = ((int) strtol((const char * restrict) & dateCopiedFromFile[3], (char ** restrict) NULL, 10) - 1);
            firstTime.tm_year = ((int) strtol((const char * restrict) & dateCopiedFromFile[6], (char ** restrict) NULL, 10) - 1900);
            nowDateString = getNowDate();
            secondTime.tm_mday = ((int) strtol((const char * restrict) & nowDateString[0], (char ** restrict) NULL, 10));
            secondTime.tm_mon = ((int) strtol((const char * restrict) & nowDateString[3], (char ** restrict) NULL, 10) - 1);
            secondTime.tm_year = ((int) strtol((const char * restrict) & nowDateString[6], (char ** restrict) NULL, 10) - 1900);
            
            if ((firstTime.tm_mday == 0 || firstTime.tm_mon == 0 || firstTime.tm_year == 0 || secondTime.tm_mday == 0 || secondTime.tm_mon == 0 || secondTime.tm_year == 0) && (errno == EINVAL || errno == ERANGE)) {
                fclose(originalFilePointer);
                fclose(tempFilePointer);
                if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
                threadAbort(STRTOL_SCOPE, STRTOL_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            }
            
            if (((scheduledVaccinationDate = mktime(& firstTime)) == -1) || ((requestVaccinationDate = mktime(& secondTime)) == -1)) {
                fclose(originalFilePointer);
                fclose(tempFilePointer);
                if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
                threadAbort(MKTIME_SCOPE, MKTIME_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            }
            
            elapsedMonths = ((((difftime(scheduledVaccinationDate, requestVaccinationDate) / 60) / 60) / 24) / 31);
            if (elapsedMonths <= MONTHS_TO_WAIT_FOR_NEXT_VACCINATION && elapsedMonths > 0) {
                strncpy((char *) newServerV_Reply->greenPassExpirationDate, (const char *) dateCopiedFromFile, DATE_LENGTH);
                isVaccineBlocked = TRUE;
            }
            break;
        }
    }
    
    if (!isVaccineBlocked) {
        strncpy((char *) newServerV_Reply->greenPassExpirationDate, (const char *) newCentroVaccinaleRequest->greenPassExpirationDate, DATE_LENGTH);
        newServerV_Reply->requestResult = TRUE;
        fclose(originalFilePointer);
        originalFilePointer = fopen(dataPath, "r");
        if (!originalFilePointer) {
            fclose(tempFilePointer);
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        }
        
        // salvo nel file serverV.dat la nuova data di scadenza della vaccinazione
        while ((getLineBytes = getline(& singleLine, & effectiveLineLength, originalFilePointer)) != -1) {
            if ((strncmp((const char *) newServerV_Reply->healthCardNumber, (const char *) singleLine, HEALTH_CARD_NUMBER_LENGTH - 1)) != 0) {
                if (fprintf(tempFilePointer, "%s", singleLine) < 0) {
                    fclose(originalFilePointer);
                    fclose(tempFilePointer);
                    if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
                    threadAbort(FPRINTF_SCOPE, FPRINTF_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
                }
            }
        }
        
        if (fprintf(tempFilePointer, "%s:%s:%s\n", newServerV_Reply->healthCardNumber, newServerV_Reply->greenPassExpirationDate, "1") < 0) {
            fclose(originalFilePointer);
            fclose(tempFilePointer);
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            threadAbort(FPRINTF_SCOPE, FPRINTF_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        }
        
        // updateFile
        fclose(originalFilePointer);
        fclose(tempFilePointer);
        if (remove(dataPath) != 0) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            threadAbort(REMOVE_SCOPE, REMOVE_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        }
        
        if (rename(tempDataPath, dataPath) != 0) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            threadAbort(RENAME_SCOPE, RENAME_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        }
        
        tempFilePointer = fopen(tempDataPath, "w+");
        if (!tempFilePointer) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
            threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        }
        fclose(tempFilePointer);
    } else {
        fclose(originalFilePointer);
        fclose(tempFilePointer);
    }
    
    if (healthCardNumberWasFound) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        
        if ((fullWriteReturnValue = fullWrite(threadConnectionFileDescriptor, (const void *) newServerV_Reply, sizeof(* newServerV_Reply))) != 0) threadAbort(FULL_WRITE_SCOPE, (int) fullWriteReturnValue, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply, nowDateString, singleLine);
        free(nowDateString);
        free(singleLine);
    } else {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
        
        if ((fullWriteReturnValue = fullWrite(threadConnectionFileDescriptor, (const void *) newServerV_Reply, sizeof(* newServerV_Reply))) != 0) threadAbort(FULL_WRITE_SCOPE, (int) fullWriteReturnValue, threadConnectionFileDescriptor, args, newCentroVaccinaleRequest, newServerV_Reply);
    }
    
    free(args);
    wclose(threadConnectionFileDescriptor);
    free(newCentroVaccinaleRequest);
    free(newServerV_Reply);
    pthread_exit(NULL);
}

void * clientS_viaServerG_RequestHandler(void * args) {
    if (pthread_mutex_lock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR);
    int threadConnectionFileDescriptor = * ((int *) args);
    if (pthread_mutex_unlock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR);
    ssize_t fullWriteReturnValue, fullReadReturnValue, getLineBytes;
    size_t effectiveLineLength = 0;
    char * singleLine = NULL, * nowDateString = NULL;
    struct tm firstTime, secondTime;
    time_t scheduledVaccinationDate = 0, requestVaccinationDate = 0;
    double elapsedMonths;
    unsigned short int greenPassStatus;
    enum boolean isGreenPassExpired = TRUE, healthCardNumberWasFound = FALSE, isGreenPassValid = FALSE;
    FILE * originalFilePointer;
    char healthCardNumber[HEALTH_CARD_NUMBER_LENGTH], dateCopiedFromFile[DATE_LENGTH], greenPassStatusString[2];

    serverV_ReplyToServerG_clientS * newServerV_Reply = (serverV_ReplyToServerG_clientS *) calloc(1, sizeof(* newServerV_Reply));
    if (!newServerV_Reply) threadAbort(CALLOC_SCOPE, CALLOC_ERROR, threadConnectionFileDescriptor, args);

    if ((fullReadReturnValue = fullRead(threadConnectionFileDescriptor, (void *) healthCardNumber, (size_t) HEALTH_CARD_NUMBER_LENGTH * sizeof(char))) != 0) threadAbort(FULL_READ_SCOPE, (int) fullReadReturnValue, threadConnectionFileDescriptor, args, newServerV_Reply);

    strncpy((char *) newServerV_Reply->healthCardNumber, (const char *) healthCardNumber, HEALTH_CARD_NUMBER_LENGTH);
    newServerV_Reply->requestResult = FALSE;

    if (pthread_mutex_lock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply);
    originalFilePointer = fopen(dataPath, "r");
    if (!originalFilePointer) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply);
        threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply);
    }

    while ((getLineBytes = getline(& singleLine, & effectiveLineLength, originalFilePointer)) != -1) {
        if ((strncmp((const char *) newServerV_Reply->healthCardNumber, (const char *) singleLine, HEALTH_CARD_NUMBER_LENGTH - 1)) == 0) {
            healthCardNumberWasFound = TRUE;
            strncpy((char *) dateCopiedFromFile, (const char *) singleLine + HEALTH_CARD_NUMBER_LENGTH, DATE_LENGTH - 1);
            dateCopiedFromFile[DATE_LENGTH - 1] = '\0';
            memset(& firstTime, 0, sizeof(firstTime));
            memset(& secondTime, 0, sizeof(secondTime));

            firstTime.tm_mday = (int) strtol((const char * restrict) & dateCopiedFromFile[0], (char ** restrict) NULL, 10);
            firstTime.tm_mon = ((int) strtol((const char * restrict) & dateCopiedFromFile[3], (char ** restrict) NULL, 10) - 1);
            firstTime.tm_year = ((int) strtol((const char * restrict) & dateCopiedFromFile[6], (char ** restrict) NULL, 10) - 1900);
            nowDateString = getNowDate();
            secondTime.tm_mday = ((int) strtol((const char * restrict) & nowDateString[0], (char ** restrict) NULL, 10));
            secondTime.tm_mon = ((int) strtol((const char * restrict) & nowDateString[3], (char ** restrict) NULL, 10) - 1);
            secondTime.tm_year = ((int) strtol((const char * restrict) & nowDateString[6], (char ** restrict) NULL, 10) - 1900);

            if ((firstTime.tm_mday == 0 || firstTime.tm_mon == 0 || firstTime.tm_year == 0 || secondTime.tm_mday == 0 || secondTime.tm_mon == 0 || secondTime.tm_year == 0) && (errno == EINVAL || errno == ERANGE)) {
                fclose(originalFilePointer);
                if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
                threadAbort(STRTOL_SCOPE, STRTOL_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
            }

            if (((scheduledVaccinationDate = mktime(& firstTime)) == -1) || ((requestVaccinationDate = mktime(& secondTime)) == -1)) {
                fclose(originalFilePointer);
                if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
                threadAbort(MKTIME_SCOPE, MKTIME_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
            }

            elapsedMonths = ((((difftime(scheduledVaccinationDate, requestVaccinationDate) / 60) / 60) / 24) / 31);
            if (elapsedMonths <= MONTHS_TO_WAIT_FOR_NEXT_VACCINATION && elapsedMonths > 0) isGreenPassExpired = FALSE;
            strncpy((char *) greenPassStatusString, (const char *) singleLine + HEALTH_CARD_NUMBER_LENGTH + DATE_LENGTH, 1);
            greenPassStatusString[1] = '\0';
            greenPassStatus = (unsigned short int) strtoul((const char * restrict) greenPassStatusString, (char ** restrict) NULL, 10);
            if ((greenPassStatus == FALSE) && (errno == EINVAL || errno == ERANGE)) {
                fclose(originalFilePointer);
                if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
                threadAbort(STRTOUL_SCOPE, STRTOUL_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
            }
            if (greenPassStatus == TRUE) isGreenPassValid = TRUE;
            break;
        }
    }

    fclose(originalFilePointer);
    if (healthCardNumberWasFound) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);

        if (isGreenPassValid && !isGreenPassExpired) newServerV_Reply->requestResult = TRUE;
        if ((fullWriteReturnValue = fullWrite(threadConnectionFileDescriptor, (const void *) newServerV_Reply, sizeof(* newServerV_Reply))) != 0) threadAbort(FULL_WRITE_SCOPE, (int) fullWriteReturnValue, threadConnectionFileDescriptor, args, newServerV_Reply, nowDateString, singleLine);
        free(nowDateString);
        free(singleLine);
    } else {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply);
        
        if ((fullWriteReturnValue = fullWrite(threadConnectionFileDescriptor, (const void *) newServerV_Reply, sizeof(* newServerV_Reply))) != 0) threadAbort(FULL_WRITE_SCOPE, (int) fullWriteReturnValue, threadConnectionFileDescriptor, args, newServerV_Reply);
    }

    free(newServerV_Reply);
    free(args);
    wclose(threadConnectionFileDescriptor);
    pthread_exit(NULL);
}

void * clientT_viaServerG_RequestHandler(void * args) {
    if (pthread_mutex_lock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR);
    int threadConnectionFileDescriptor = * ((int *) args);
    if (pthread_mutex_unlock(& connectionFileDescriptorMutex) != 0) threadRaiseError(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR);
    ssize_t fullWriteReturnValue, fullReadReturnValue, getLineBytes;
    size_t effectiveLineLength = 0;
    char * singleLine = NULL;;
    enum boolean healthCardNumberWasFound = FALSE;
    FILE * originalFilePointer, * tempFilePointer = NULL;
    char healthCardNumber[HEALTH_CARD_NUMBER_LENGTH], dateCopiedFromFile[DATE_LENGTH];

    serverG_RequestToServerV_onBehalfOfClientT * newServerG_Request = (serverG_RequestToServerV_onBehalfOfClientT *) calloc(1, sizeof(* newServerG_Request));
    if (!newServerG_Request) threadAbort(CALLOC_SCOPE, CALLOC_ERROR, threadConnectionFileDescriptor, args);

    serverV_ReplyToServerG_clientT * newServerV_Reply = (serverV_ReplyToServerG_clientT *) calloc(1, sizeof(* newServerV_Reply));
    if (!newServerV_Reply) threadAbort(CALLOC_SCOPE, CALLOC_ERROR, threadConnectionFileDescriptor, args, newServerG_Request);

    if ((fullReadReturnValue = fullRead(threadConnectionFileDescriptor, (void *) newServerG_Request, sizeof(* newServerG_Request))) != 0) {
        threadAbort(FULL_READ_SCOPE, (int) fullReadReturnValue, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
    }

    strncpy((char *) healthCardNumber, (const char *) newServerG_Request->healthCardNumber, HEALTH_CARD_NUMBER_LENGTH);
    strncpy((char *) newServerV_Reply->healthCardNumber, (const char *) healthCardNumber, HEALTH_CARD_NUMBER_LENGTH);
    newServerV_Reply->updateResult = FALSE;

    if (pthread_mutex_lock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_LOCK_SCOPE, PTHREAD_MUTEX_LOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
    originalFilePointer = fopen(dataPath, "r");
    if (!originalFilePointer) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerV_Reply);
        threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
    }

    while ((getLineBytes = getline(& singleLine, & effectiveLineLength, originalFilePointer)) != -1) {
        if ((strncmp((const char *) newServerV_Reply->healthCardNumber, (const char *) singleLine, HEALTH_CARD_NUMBER_LENGTH - 1)) == 0) {
            healthCardNumberWasFound = TRUE;
            strncpy((char *) dateCopiedFromFile, (const char *) singleLine + HEALTH_CARD_NUMBER_LENGTH, DATE_LENGTH - 1);
            dateCopiedFromFile[DATE_LENGTH - 1] = '\0';
            break;
        }
    }

    fclose(originalFilePointer);
    if (healthCardNumberWasFound) {
        originalFilePointer = fopen(dataPath, "r");
        tempFilePointer = fopen(tempDataPath, "w");
        if (!originalFilePointer || !tempFilePointer) {
            fclose(originalFilePointer);
            fclose(tempFilePointer);
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
            threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
        }

        // salvo nel file serverV.dat il nuovo stato del green pass
        while ((getLineBytes = getline(& singleLine, & effectiveLineLength, originalFilePointer)) != -1) {
            if ((strncmp(newServerV_Reply->healthCardNumber, singleLine, HEALTH_CARD_NUMBER_LENGTH - 1)) != 0) {
                if (fprintf(tempFilePointer, "%s", singleLine) < 0) {
                    fclose(originalFilePointer);
                    fclose(tempFilePointer);
                    if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
                    threadAbort(FPRINTF_SCOPE, FPRINTF_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
                }
            }
        }

        if (fprintf(tempFilePointer, "%s:%s:%hu\n", newServerV_Reply->healthCardNumber, dateCopiedFromFile, newServerG_Request->updateValue) < 0) {
            fclose(originalFilePointer);
            fclose(tempFilePointer);
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
            threadAbort(FPRINTF_SCOPE, FPRINTF_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
        }

        // updateFile
        fclose(originalFilePointer);
        fclose(tempFilePointer);
        if (remove(dataPath) != 0) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
            threadAbort(REMOVE_SCOPE, REMOVE_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
        }

        if (rename(tempDataPath, dataPath) != 0) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
            threadAbort(RENAME_SCOPE, RENAME_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
        }

        tempFilePointer = fopen(tempDataPath, "w+");
        if (!tempFilePointer) {
            if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
            threadAbort(FOPEN_SCOPE, FOPEN_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply, singleLine);
        }
        fclose(tempFilePointer);
        newServerV_Reply->updateResult = TRUE;
        free(singleLine);
    }

    if ((fullWriteReturnValue = fullWrite(threadConnectionFileDescriptor, (const void *) newServerV_Reply, sizeof(* newServerV_Reply))) != 0) {
        if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
        threadAbort(FULL_WRITE_SCOPE, (int) fullWriteReturnValue, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
    }

    if (pthread_mutex_unlock(& fileSystemAccessMutex) != 0) threadAbort(PTHREAD_MUTEX_UNLOCK_SCOPE, PTHREAD_MUTEX_UNLOCK_ERROR, threadConnectionFileDescriptor, args, newServerG_Request, newServerV_Reply);
    wclose(threadConnectionFileDescriptor);
    free(newServerV_Reply);
    free(newServerG_Request);
    free(args);
    pthread_exit(NULL);
}

void threadAbort (char * errorScope, int exitCode, int threadConnectionFileDescriptor, void * arg1, ...) {
    wclose(threadConnectionFileDescriptor);
    va_list argumentsList;
    void * currentElement;
    free(arg1);
    va_start(argumentsList, arg1);
    while ((currentElement = va_arg(argumentsList, void *)) != 0) free(currentElement);
    va_end(argumentsList);
    threadRaiseError(errorScope, exitCode);
}
