/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#define __ASM__
#include <consts.h>
#include <cos_asm_server_stub_simple_stack.h>

.text
cos_asm_server_stub(srv_dummy_hello);
cos_asm_server_stub_rets(srv_dummy_goodbye);