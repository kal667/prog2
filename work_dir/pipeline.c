
/*
 * pipeline.c
 * 
 * Donald Yeung
 */


#include <stdlib.h>
#include "fu.h"
#include "pipeline.h"


int
commit(state_t *state) {
}


void
writeback(state_t *state) {
}


void
execute(state_t *state) {
}


int
memory_disambiguation(state_t *state) {
}


int
issue(state_t *state) {
}


int
dispatch(state_t *state) {

	/*Return -1 for stall, 1 for halt or NOP, 0 for issue*/

	int instr, use_imm, tag_check; /*Holds instruction, immediate flag, check tags*/
	const op_info_t *op_info;
	unsigned int opcode, func, r1, r2, r3, immu; /*Unsigned Macros*/
	signed int imm, offset; /*Signed Macros*/

	/*Get instruction from if_id pipeline register*/
	instr = state->if_id.instr;

	/*Perform all macros for later use*/
	opcode = FIELD_OPCODE(instr);
	func = FIELD_FUNC(instr);
	r1 = FIELD_R1(instr);
	r2 = FIELD_R2(instr);
	r3 = FIELD_R3(instr);
	imm = FIELD_IMM(instr);
	immu = FIELD_IMMU(instr);
	offset = FIELD_OFFSET(instr);

	/*Decode Instruction in IF/ID*/
	op_info = decode_instr(instr, &use_imm);

	/*Squash NOPs*/
	if (instr == 0) {
		return 0;
	}

	/*
	ROB
	-?Check to see if ROB is full?
	-Insert instructions onto ROB at tail
	-Set completed field to FALSE (except for HALT = TRUE)
	-Increment ROB_tail (moved to end)
	*/
	state->ROB[state->ROB_tail].instr = instr;
	if (op_info->fu_group_num == FU_GROUP_HALT) state->ROB[state->ROB_tail].completed = TRUE;
	else state->ROB[state->ROB_tail].completed = FALSE;

	/*
	IQ
	-?Check to see if IQ is full?
	-Insert instruction and pc onto IQ at tail (except HALT)
	-Set issued field to FALSE
	-Increment IQ_tail (moved to end)
	-Set ROB_index field
	-Set tag1/operand1 and tag2/operand2 fields
		-tag = -1 if value is ready and present
		-tag = ROB_index of in-flight instruction if value is being computed
	*/

		/*Halt: fetch lock and return*/
	if (op_info->fu_group_num == FU_GROUP_HALT) {
		state->fetch_lock = TRUE;
		return 1;
	}
		/*Insert instruction and pc onto IQ*/
	state->IQ[state->IQ_tail].instr = instr;
	state->IQ[state->IQ_tail].pc = state->pc;

		/*Issued field to FALSE*/
	state->IQ[state->IQ_tail].issued = FALSE;

		/*Set ROB_index field*/
	state->IQ[state->IQ_tail].ROB_index = state->ROB_tail;

		/*
		Set tag1/operand1 and tag2/operand2 fields
		-Check desired register
			-if (tag_check == -1) 
				-read register value into operand
				-set IQ tag = -1
			-else -> tag_check = ROB_index of in-flight instruction using register 
				-if (ROB_index in-flight Completed flag == TRUE)
					-read result into operand
					-set IQ tag = -1
				-else -> In-flight instruction has not completed
					-IQ tag = tag_check
		-Set destination register tag to ROB_index
		*/
			/*Integer Arithmetic / Logic*/
	if (op_info->fu_group_num == FU_GROUP_INT) {
		/*Check R1 tag*/
		tag_check = check_in_flight_status()
		if(tag_check == -1){
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int[r1].w;
			}
			state->IQ[state->IQ_tail].tag1 = tag_check;
		}
		/*Check R2 tag if no immediate*/
		if(use_imm == FALSE) {
			tag_check = check_in_flight_status(state);
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand2.integer.w = state->rf_int.reg_int[r2].w;
				state->IQ[state->IQ_tail].tag2 = -1
			}
			else{
				tag_check = state->ROB[tag_check].completed;
				if (tag_check == -1){
					state->IQ[state->IQ_tail].operand2.integer.w = state->rf_int.reg_int[r2].w;
			}
			state->IQ[state->IQ_tail].tag2 = tag_check;
			}
				/*Set destination register R3 tag to ROB_index*/
			state->rf_int.tag[r3] = state->ROB_tail;
		}
			/*Integer Arithmetic / Logic with Immediate*/
		else {
			state->IQ[state->IQ_tail].operand2.integer.w = imm;
			state->IQ[state->IQ_tail].tag2 = -1;
				/*Set destination register R2 tag to ROB_index*/
			state->rf_int.tag[r2] = state->ROB_tail;
		}
	}

			/*Memory*/
	if (op_info->fu_group_num == FU_GROUP_MEM) {
		/*?Is this the same for Load and Store?*/
			/*Check R1 tag*/
		tag_check = check_in_flight_status()
		if(tag_check == -1){
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int[r1].w;
			}
			state->IQ[state->IQ_tail].tag1 = tag_check;
		}

			/*Second operand is an immediate and always ready*/
		state->IQ[state->IQ_tail].operand2.integer.w = imm;
		state->IQ[state->IQ_tail].tag2 = -1;
			
			/*Set destination register tag to ROB_index*/
		if (op_info->data_type == DATA_TYPE_F){
			state->rf_fp.tag[r2] = state->ROB_tail;
		}
		else {
			state->rf_int.tag[r2] = state->ROB_tail;
		}
	}

			/*Floating Point Arithmetic*/
	if ((op_info->fu_group_num == FU_GROUP_ADD)||(op_info->fu_group_num == FU_GROUP_MULT)||(op_info->fu_group_num == FU_GROUP_DIV)){
				/*Check R1 tag*/
		tag_check = check_in_flight_status();
		if(tag_check == -1) {
			state->IQ[state->IQ_tail].operand1.flt = state->rf_fp.reg_fp[r1];
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand1.flt = state->rf_fp.reg_int[r1];
			}
			state->IQ[state->IQ_tail].tag1 = tag_check;
		}
				/*Check R2 tag*/
		tag_check = check_in_flight_status();
		if(tag_check == -1) {
			state->IQ[state->IQ_tail].operand2.flt = state->rf_fp.reg_fp[r2];
			state->IQ[state->IQ_tail].tag2 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand2.flt = state->rf_fp.reg_int[r2];
			}
			state->IQ[state->IQ_tail].tag2 = tag_check;
		}
				/*Set destination register R3 tag to ROB_index*/
		state->rf_fp.tag[r3] = state->ROB_tail;
	}

			/*Control*/
	if (op_info->fu_group_num == FU_GROUP_BRANCH){
				/*These instructions don't have a source register*/
		if ((op_info->operation == OPERATION_J)||(op_info->operation == OPERATION_JAL)) {
			state->IQ[state->IQ_tail].operand1.integer.w = 0;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
				/*Check R1 tag*/
		else{
			tag_check = check_in_flight_status();
			if(tag_check == -1) {
				state->IQ[state->IQ_tail].operand1.integer.wu = state->rf_int.reg_int[r1].wu;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else{
				tag_check = state->ROB[tag_check].completed;
				if (tag_check == -1){
					state->IQ[state->IQ_tail].operand1.integer.wu = state->rf_int.reg_int[r1].wu;
				}
				state->IQ[state->IQ_tail].tag1 = tag_check;
			}
		}
				/*Second operand is always an immediate*/
		state->IQ[state->IQ_tail].operand2.integer.w = imm;
		state->IQ[state->IQ_tail].tag2 = -1;

		/*
		Jumps and branches don't write destination registers,they 
		shouldn't create a new register name in the resiter file tag
		(Excet for JAL - ignored for now)
		*/
	}

	
	/*Increment ROB and IQ tail*/
	incr_ROB_tail(state);
	incr_IQ_tail(state);

	/*Ensure that R0 tag is always -1*/
	state->rf_int.tag[0] = -1;

}


void
fetch(state_t *state) {

	/*Go to memory and get instruction | 32 bits wide*/
	state->if_id.instr = (state->mem[state->pc]<<24)|(state->mem[state->pc+1]<<16)|(state->mem[state->pc+2]<<8)|(state->mem[state->pc+3]);
	state->if_id.pc = state->pc;

	/*Increment the PC*/
	state->pc += 4;
}

/*Function to increment ROB_tail*/
void
incr_ROB_tail(state_t *state) {

	if (state->ROB_tail == ROB_SIZE -1) state->ROB_tail = 0;
	else state->ROB_tail = state->ROB_tail + 1;
}

/*Function to increment IQ_tail*/
void
incr_IQ_tail(state_t *state) {

	if (state->IQ_tail == IQ_SIZE -1) state->IQ_tail = 0;
	else state->IQ_tail = state->IQ_tail + 1;
}
