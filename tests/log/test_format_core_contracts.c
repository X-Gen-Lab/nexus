#include "log/log_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    log_record_t r = {.level = LOG_LEVEL_WARN, .module = "driver", .file = "/src/uart.c",
        .function = "poll", .line = 42, .message = "fault", .timestamp_ms = 123,
        .clock_text = "01:02:03"};
    log_format_options_t o = {.pattern = "%T %t %L %l %M %F %f %n %m %%"};
    char text[128];
    assert(log_format_record(text, sizeof(text), &o, &r) == strlen("123 01:02:03 WARN W driver uart.c poll 42 fault %"));
    assert(strcmp(text, "123 01:02:03 WARN W driver uart.c poll 42 fault %") == 0);
    char guarded[8] = "xxxxxxx";
    assert(log_format_record(guarded + 1, 4, &o, &r) == 3);
    assert(guarded[0] == 'x' && guarded[4] == '\0' && guarded[5] == 'x');
    r.level = (log_level_t)-1;
    assert(log_format_record(text, sizeof(text), &o, &r) == 0 && text[0] == '\0');
    r.level = LOG_LEVEL_INFO; r.clock_text = NULL; o.pattern = "%t";
    assert(log_format_record(text, sizeof(text), &o, &r) == 8 && strcmp(text, "--:--:--") == 0);
    assert(log_format_record(NULL, 0, &o, &r) == 0);
    puts("stateless formatter normal/bounds/invalid/time-port contracts passed"); return 0;
}
