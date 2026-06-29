#include "platform_touch.h"

#include "platform.h"
#include "smartwatch_board.h"

bool platform_touch_is_ready(void)
{
    return platform_get_status()->touch_ready;
}

bool platform_touch_wakeup_pending(void)
{
    return smartwatch_board_touch_wakeup_pending();
}
