#include "util.h"

char* format_text(const char *format, ...) {
    free(format_scratch);

    va_list args;

    va_start(args, format);
    if(0 > vasprintf(&format_scratch, format, args)) format_scratch = NULL;    //this is for logging, so failed allocation is not fatal
    va_end(args);

    if(format_scratch) {
      return format_scratch;
    } else {
      return "uh oh";
    }
}