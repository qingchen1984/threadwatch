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

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "config.h"
#include "events.h"
#include "global.h"

RingBuffer *createRingBuffer() 
{
#ifdef DEBUG_STRUCTURE
    DataRecord startEvent;

    printf("sizeof(DataRecord): %d\n",sizeof(DataRecord));
    printf("sizeof(ThreadStartEvent): %d\n",sizeof(ThreadStartEvent));
    printf("sizeof(ThreadDeathEvent): %d\n",sizeof(ThreadDeathEvent));
    printf("sizeof(ThreadStateChangeEvent): %d\n",sizeof(ThreadStateChangeEvent));
    printf("sizeof(timestamp): %d\n",sizeof(struct timespec));
    printf("Offset thread name: %d\n",(void*) &startEvent.startEvent.threadName[0] - (void*) &startEvent);
    printf("Offset uniqueThreadId: %d\n",(void*) &startEvent.uniqueThreadId - (void*) &startEvent);
    printf("Offset timestamp: %d\n",(void*) &startEvent.timestamp - (void*) &startEvent);
    printf("Offset startEvent: %d\n",(void*) &startEvent.startEvent - (void*) &startEvent);
    printf("sizeof(jint): %d\n", sizeof(jint));
    printf("Offset threadState: %d\n",(void*) &startEvent.stateChangeEvent - (void*) &startEvent);
#endif

#ifdef DEBUG_BUFFER
    printf("Creating ringbuffer with size %d\n",sizeof(RingBuffer));
#endif

    RingBuffer *result = (RingBuffer*) malloc( sizeof( RingBuffer ) );
    
    if ( ! result ) {
       fprintf(stderr,"Failed to allocate ringbuffer");
       abort();
    }
    memset(result,0,sizeof(RingBuffer) );
    
    result -> records = (DataRecord*) malloc( SAMPLE_RINGBUFFER_SIZE * sizeof(DataRecord) );
    if ( ! result->records ) 
    {
       free(result);
       fprintf(stderr,"Failed to allocate memory for ringbuffer data elements");
       abort();        
    }
    memset(result->records,0, SAMPLE_RINGBUFFER_SIZE * sizeof(DataRecord) );
    
    pthread_mutex_init(&result->lock, NULL);    

#ifdef DEBUG_BUFFER
    printf("Ringbuffer created.\n");
#endif
    
    return result;
}

static void lock(RingBuffer *buffer) {
      pthread_mutex_lock( &buffer->lock );
}

static void unlock(RingBuffer *buffer) {
      pthread_mutex_unlock( &buffer->lock );
}

void destroyRingBuffer(RingBuffer *buffer) 
{             
    printf("INFO: Disposing ring buffer (events written: %d , events read: %d)\n",buffer->elementsWritten,buffer->elementsRead);
    
    if ( buffer ->lostSamplesCount > 0 ) {
       printf("WARNING: Lost %d samples\n",buffer->lostSamplesCount);
    }
    free(buffer->records);
    free(buffer);
}

int writeRecord(RingBuffer *buffer, WriteRecordCallback callback , void* data)
{
    int bufferNotFull;
    int newPtr;
    
#ifdef DEBUG_BUFFER
    printf("About to write to ring buffer\n");
#endif
    // START CRITICAL SECTION    
    lock(buffer);
    
    newPtr = (buffer->writePtr+1) % SAMPLE_RINGBUFFER_SIZE;
    bufferNotFull = newPtr != buffer->readPtr;
    if ( bufferNotFull ) 
    {
       if ( callback( &buffer->records[ buffer->writePtr] , data ) ) {
#ifdef DEBUG
    printf("Callback set record type: %d\n\n",buffer->records[buffer->writePtr].type);
#endif           
         buffer->elementsWritten++;    
         buffer->writePtr = newPtr;
       }
    } else {
      buffer -> lostSamplesCount++;    
    }
    
    // END CRITICAL SECTION
    unlock(buffer);
#ifdef DEBUG_BUFFER
    printf("written to ring buffer ( buffer_not_full = %d)\n",bufferNotFull);
#endif    
    return bufferNotFull;
}

int readRecord(RingBuffer *buffer, void (*callback)(DataRecord*) ) 
{
    int bufferNotEmpty;
    
    // START CRITICAL SECTION    
    lock(buffer);   
    
    bufferNotEmpty = buffer->readPtr != buffer->writePtr;
    if ( bufferNotEmpty ) 
    {
        callback( &buffer->records[buffer->readPtr] );
        buffer->elementsRead++;        
        buffer->readPtr = (buffer->readPtr+1) % SAMPLE_RINGBUFFER_SIZE;
    }
    
    // END CRITICAL SECTION    
    unlock(buffer);    
    return bufferNotEmpty;
}
