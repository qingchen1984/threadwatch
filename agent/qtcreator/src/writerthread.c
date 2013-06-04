/*
Copyright 2013 Tobias Gierke <tobias.gierke@code-sourcery.de>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "global.h"
#include "writerthread.h"
#include "events.h"

static pthread_t writerThreadId;
static FILE *outputFile = NULL;

static volatile int terminateWriter = 0;

static void *writerThread(RingBuffer *buffer);

void startWriterThread(RingBuffer *buffer,char *file)
{
  int err;
  unsigned int magic = FILEHEADER_MAGIC;
  pthread_attr_t tattr;
  
  if ( configuration.verboseMode ) {
    printf("INFO: Started writer thread that writes to file %s\n",file);
  }

    if ( ! (outputFile=fopen(file,"w+") ) ) 
    {
      fprintf(stderr,"ERROR: Failed to open output file %s\n",file);
      abort();
    }  

  // write header
   if ( fwrite(&magic,1,sizeof(magic),outputFile) != sizeof(magic) ) {
      fprintf(stderr,"ERROR: Failed to write to output file %s\n",file);
      abort();
   }
     
  /* initialized with default attributes */
  pthread_attr_init(&tattr);

  /* call an appropriate functions to alter a default value */
  pthread_attr_setstacksize(&tattr , 1 * 1024 * 1024);

  if ( ( err = pthread_create( &writerThreadId , &tattr , (void *(*)(void *)) &writerThread , buffer ) ) ) {
        fprintf( stderr,"ERROR: Failed to create pthread , err=%d\n", err);
        abort();
  }    
}

static void writeRecordToFile(DataRecord *record) 
{
    int bytesToWrite;
    int bytesWritten;
        
    switch( record->type ) 
    {
        case EVENT_THREAD_START:
            bytesToWrite = THREAD_START_EVENT_SIZE;
            break;
        case EVENT_THREAD_DEATH:
            bytesToWrite = THREAD_DEATH_EVENT_SIZE;
            break;
        case EVENT_THREAD_SAMPLE:
            bytesToWrite = THREAD_STATE_CHANGE_EVENT_SIZE;
            break;
        default:
            fprintf(stderr,"ERROR: Internal error, don't know how to write record with type %d\n",record->type);
            abort();
    }

#ifdef DEBUG
    printf("Writing record to file (type: %d , offset: %ld, bytes: %d)\n",record->type,currentFileOffset,bytesToWrite);
    fflush(stdout);
#endif

    bytesWritten = fwrite( record , 1 , bytesToWrite , outputFile );
    if ( bytesWritten != bytesToWrite ) 
    {
        fprintf(stderr,"ERROR: Failed to write %d bytes to output file\n",bytesToWrite);
        abort();           
    }
}

static void *writerThread(RingBuffer *buffer) 
{      
#ifdef DEBUG
    printf("Writer thread is running.");
#endif
    do 
    {
        while( readRecord( buffer , &writeRecordToFile ) );
        usleep( WRITERTHREAD_SLEEP_TIME_MILLIS * 1000);
      __sync_synchronize();        
    } while ( ! terminateWriter );
    
#ifdef DEBUG
    printf("Writer thread is terminating.");
#endif
    
    while( readRecord( buffer , &writeRecordToFile ) );
        
    fflush(outputFile);
    fclose(outputFile);

    if ( configuration.verboseMode ) {
       printf("INFO: Closed output file.\n");
    }
    return NULL;
}

void terminateWriterThread() 
{    
#ifdef DEBUG
    printf("Asking writer thread to terminate.");
#endif
    
   __sync_synchronize();
   terminateWriter = 1;  
   
   if ( outputFile != NULL && pthread_join( writerThreadId , NULL ) ) 
   {
     fprintf(stderr,"ERROR: Failed to join() with writer thread");    
   }
}
