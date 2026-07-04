#pragma once

#define DEFAULT_LAYER_CMD_SELECT 0
#define DEFAULT_LAYER_CMD_NEXT 1

#define DF_SEL DEFAULT_LAYER_CMD_SELECT
/* &df has #binding-cells = <2>, so DF_INC (used bare in keymaps as
 * `&df DF_INC`) must expand to two cells. The second value is an unused
 * placeholder - the NEXT command ignores param2. */
#define DF_INC DEFAULT_LAYER_CMD_NEXT 1
