//
//  clientS.h
//  VanillaGreenPass
//
//  Created by Denny Caruso on 10/01/22.
//

#ifndef clientS_h
#define clientS_h

#include "GreenPassUtility.h"

void checkGreenPass     (int serverG_SocketFileDescriptor, const void * healthCardNumber, size_t nBytes);

#endif /* clientS_h */