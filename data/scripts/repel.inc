EventScript_RepelWoreOff::
        lockall
        checkitem ITEM_MAX_REPEL
        goto_if_eq VAR_RESULT, TRUE, EventScript_RepelAsk
        checkitem ITEM_SUPER_REPEL
        goto_if_eq VAR_RESULT, TRUE, EventScript_RepelAsk
        checkitem ITEM_REPEL
        goto_if_eq VAR_RESULT, TRUE, EventScript_RepelAsk
        msgbox Text_RepelWoreOff, MSGBOX_SIGN
        releaseall
        end

EventScript_RepelAsk:
        msgbox Text_RepelWoreOffUseAnother, MSGBOX_YESNO
        goto_if_eq VAR_RESULT, YES, EventScript_UseRepel
        releaseall
        end

EventScript_UseRepel:
        special TryUseNextRepel
        goto_if_eq VAR_RESULT, FALSE, EventScript_NoRepel
        msgbox gText_UsedVar2WildRepelled, MSGBOX_DEFAULT
        releaseall
        end

EventScript_NoRepel:
        msgbox Text_RepelWoreOff, MSGBOX_SIGN
        releaseall
        end

Text_RepelWoreOff:
        .string "REPEL's effect wore off…$"

Text_RepelWoreOffUseAnother:
        .string "REPEL's effect wore off.\pWould you like to use another?$"
