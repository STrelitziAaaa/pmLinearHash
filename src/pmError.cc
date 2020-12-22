
#include "pmError.h"
#include <mutex>

thread_local pmError pmError_tls;
std::mutex pmErrorPrintMutx;