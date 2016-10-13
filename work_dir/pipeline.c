
/*
 * pipeline.c
 * 
 * Donald Yeung
 */


#include <stdlib.h>
#include "fu.h"
#include "pipeline.h"


int
commit(state_t *state, int *num_insn) {
	
	/*Return TRUE for HALT, FALSE otherwsie*/

	/*
	If instruction at ROB head has completed
		Write result to register file (INT of FP)
		Check if instruction's ROB index == tag in register being written
			If they are equal then set tag = -1
		Loads commit to register file but stores do not
		Stores perform their operation to memory
			Call issue_fu_mem an copy value to memory
		Committing a HALT instruction terminates the simulation
		Increment ROB_head
	Else exit
	*/

	int instr, use_imm, issue;
	operand_t result, target;
	const op_info_t *op_info;
	
	if (state->ROB[ROB_head].completed == TRUE) {

		instr = state->ROB[ROB_head].instr;
		result = state->ROB[ROB_head].result;
		target = state->ROB[ROB_head].target;
		op_info = decode_instr(instr, &use_imm);

		/*Perform Register macros for later use*/
		r1 = FIELD_R1(instr);
		r2 = FIELD_R2(instr);
		r3 = FIELD_R3(instr);

		/*HALT Condition*/
		if (op_info->fu_group_num == FU_GROUP_HALT) return TRUE;

			/*Floating Point Commit*/
		if (op_info->data_type == DATA_TYPE_F) {
				/*FP Memory*/
			if (op_info->fu_group_num == FU_GROUP_MEM) {
					/*Loads*/
				if (op_info->operation == OPERATION_LOAD) {
					union IntFloat val;
					val.i = ((state->mem[target.integer.w]<<24)|(state->mem[target.integer.w+1]<<16)|(state->mem[target.integer.w+2]<<8)|(state->mem[target.integer.w+3]));
					state->rf_fp.reg_fp.flt[r2] = val.f;
					/*Set ready tag*/
					if (ROB_head == state->rf_fp.tag[r2]) state->rf_fp.tag[r2] = -1;
					*num_insn += 1;
				}
					/*Stores*/
				if (op_info->operation == OPERATION_STORE) {
					issue = issue_fu_mem(*fu_mem_list, ROB_head,target.integer.wu, TRUE);
						/*Succesful Issue -> Copy to Memory*/
					if (issue == 0){
						union IntFloat val;
						val.f = state->rf_fp.reg_fp[r2];
						state->mem[target.integer.w] = (val.i >> 24) & 0xFF;
						state->mem[target.integer.w + 1] = (val.i >> 16) & 0xFF;
						state->mem[target.integer.w + 2] = (val.i >> 8) & 0xFF;
						state->mem[target.integer.w + 3] = val.i & 0xFF;
						*num_insn += 1;
					}
					else return FALSE;
				}
			}
				/*FP Arithmetic*/
			else {
				state->rf_fp.reg_fp.flt[r3] = result.flt;
					/*Set ready tag*/
				if (ROB_head == state->rf_fp.tag[r3]) state->rf_fp.tag[r3] = -1;
				*num_insn += 1;
			}
		}

		/*Integer Commit*/
		/*INT Loads and Stores*/
		if (op_info->data_type == DATA_TYPE_W) {
			/*Loads*/
			if (op_info->operation == OPERATION_LOAD){
				state->rf_int.reg_int.integer[r2].w = state->mem[result.integer.w];
				/*Set ready tag*/
				if (ROB_head == state->rf_int.tag[r2]) state->rf_int.tag[r2] = -1;
				*num_insn += 1;
			}
			/*Stores*/
			if (op_info->operation == OPERATION_STORE) {
				issue = issue_fu_mem(*fu_mem_list, ROB_head,target.integer.wu, TRUE);
				/*Succesful Issue -> Copy to Memory*/
				if (issue == 0){
					state->mem[target.integer.w] = state->rf_int.reg_int.integer[r2].w;
					*num_insn += 1;
				}
				else return FALSE;
			}
		}

		/*Integer Arithmetic / Logic*/
		if (op_info->fu_group_num == FU_GROUP_INT) {
			if (use_imm == FALSE){
				state->rf_int.reg_int.integer[r3].w = result.integer.w;
				*num_insn += 1;
			}
			else {
				state->rf_int.reg_int.integer[r2].w = result.integer.w;
				*num_insn += 1;
			}
		}

		/*Increment ROB_head*/
		state->ROB_head = (state->ROB_head + 1) % ROB_SIZE;
	}
	
	else return FALSE;
}


void
writeback(state_t *state) {

	int i, index;

	/*Integer*/
	for (i = 0; i < state->wb_port_int_num; i++) {
    	if (state->wb_port_int[i].tag != -1) {
      		/*Handle all non-mem operations*/
      		if (state->wb_port_int[i].tag < ROB_SIZE){
      			state->ROB[state->wb_port_int[i].tag].completed = TRUE;
      			/*Check tag against IQ*/
      			for (index = state->IQ_head; index != state->IQ_tail; index = (index + 1) & (IQ_SIZE-1)) {
      				/*Tag 1 Match -> copy result to operand field and set tag = -1*/
      				if (state->IQ[index].tag1 == state->wb_port_int[i].tag) {
      					state->IQ[index].operand1 = state->ROB[state->wb_port_int[i].tag].result.integer.w;
      					state->IQ[index].tag1 = -1;
      				}
      				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
      				if (state->IQ[index].tag2 == state->wb_port_int[i].tag) {
      					state->IQ[index].operand2 = state->ROB[state->wb_port_int[i].tag].result.integer.w;
      					state->IQ[index].tag2 = -1;
      				}
      			}
      			/*Check tag against CQ*/
      			for (index = state->CQ_head; index != state->CQ_tail; index = (index + 1) & (CQ_SIZE-1)) {
      				/*Tag 1 Match -> copy result to operand field and set tag = -1*/
      				if (state->CQ[index].tag1 == state->wb_port_int[i].tag) {
      					state->CQ[index].operand1 = state->ROB[state->wb_port_int[i].tag].result.integer.w;
      					state->CQ[index].tag1 = -1;
      				}
      				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
      				if (state->CQ[index].tag2 == state->wb_port_int[i].tag) {
      					state->CQ[index].operand2 = state->ROB[state->wb_port_int[i].tag].result.integer.w;
      					state->CQ[index].tag2 = -1;
      				}
      			}
      		}
      		/*Handle all mem operations*/
      		else{

      		}
      		/*Clear WB INT tag*/
      		state->wb_port_int[i].tag = -1;
      	}
    }

    /*Floating Point*/
    for (i = 0; i < state->wb_port_fp_num; i++) {
    	if (state->wb_port_fp[i].tag != -1) {
      		state->ROB[state->wb_port_fp[i].tag].completed = TRUE;
  			/*Check tag against IQ*/
  			for (index = state->IQ_head; index != state->IQ_tail; index = (index + 1) & (IQ_SIZE-1)) {
  				/*Tag 1 Match -> copy result to operand field and set tag = -1*/
  				if (state->IQ[index].tag1 == state->wb_port_fp[i].tag) {
  					state->IQ[index].operand1 = state->ROB[state->wb_port_fp[i].tag].result.flt
  					state->IQ[index].tag1 = -1;
  				}
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->IQ[index].tag2 == state->wb_port_fp[i].tag) {
  					state->IQ[index].operand2 = state->ROB[state->wb_port_fp[i].tag].result.flt
  					state->IQ[index].tag2 = -1;
  				}
  			}
  			/*Check tag against CQ*/
  			for (index = state->CQ_head; index != state->CQ_tail; index = (index + 1) & (CQ_SIZE-1)) {
  				/*Tag 1 should never match*/
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->CQ[index].tag2 == state->wb_port_flt[i].tag) {
  					state->CQ[index].operand2 = state->ROB[state->wb_port_int[i].tag].result.flt
  					state->CQ[index].tag2 = -1;
  				}
  			}
    		/*Clear WB FP tag*/
  			state->wb_port_fp[i].tag = -1;
    	}
    }

    /*Branch*/
    if (state->branch_tag != -1) {
    	state->ROB[state->branch_tag].completed = TRUE;
    	
    	/*This needs some work*/
		/*We can say result of 1 -> Take branch*/
		if (state->ROB[state->branch_tag].result.integer.w == 1) {
			state->pc = state->ROB[state->branch_tag].target.integer.wu;
			state->if_id.instr = 0;
		}
		state->branch_tag = -1; /*Clear Branch Tag*/
		
    	state->fetch_lock = FALSE;
    }
}


void
execute(state_t *state) {
	advance_fu_mem(state->fu_mem_list, state->wb_port_int, state->wb_port_int_num, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_int(state->fu_int_list, state->wb_port_int, state->wb_port_int_num, state->branch_tag);
	advance_fu_fp(state->fu_add_list, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_fp(state->fu_mult_list, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_fp(state->fu_div_list, state->wb_port_fp, state->wb_port_fp_num);
}


int
memory_disambiguation(state_t *state) {
	/*
	The memory disambiguation stage scans the “CQ” for ready 
	memory operations, checks for con-flicts, and issues the 
	memory operations if no conflicts exist
	
	-For(Scan CQ)
		-If Store
			-If tag1 != -1
				-Disambiguation fails -> no memory operations can issue
				-break
			-If unissued && tag1 && tag2 == -1
				-Issue store immediately
					-Set completed bit in coresponding ROB entry to TRUE
					-Place the store value in the result field for ROB entry
					-break
		-If Load
			-If unissued && tag1 == -1
				Check for conflicts with an older store
				-Rescan the CQ from head to see if earlier store is writing to the same location
				-If conflict
					-Continue scanning CQ
				-Else
					-Try to issue_fu_mem
					-If issue_fu_mem == fail
						-Stall -> Exit
					-if issue_fu_mem == success
						-Set issued bit to TRUE
						-Perform Operation
		-If pointer == tail
			-break
	*/

	int pointer, rescan, conflict, issue;

	pointer = state->CQ_head;
	
	for (;;) {
		conflict = FALSE;
		
		/*Stores*/
		if (state->CQ[pointer].store == TRUE){
			/*No Effective Address*/
			if (state->CQ[pointer].tag1 != -1) {
				break;
			}
			/*Unissued and Ready*/
			if ((state->CQ[pointer].issued == FALSE) && (state->CQ[pointer].tag1 == -1) && (state->CQ[pointer].tag2 == -1)){
				state->ROB[state->CQ[pointer].ROB_index].completed = TRUE;
				state->ROB[state->CQ[pointer].ROB_index].result = state->rf_fp.reg_fp.flt[r2];
				break;
			}
		}
		/*Loads*/
		if (state->CQ[pointer].store == FALSE){
			/*Unissued and Ready*/
			if ((state->CQ[pointer].issued == FALSE) && (state->CQ[pointer].tag1 == -1)){
				/*Check for conflicts with an older store*/
				for (rescan = state->CQ_head; !(rescan == pointer); rescan = (rescan + 1) % CQ_SIZE ){
					/*If an unissued store is writing the same location*/
					if ((state->CQ[rescan].store == TRUE) && (state->CQ[rescan].issued == FALSE) && (state->CQ[rescan].)) {
						/*Set conflict flag and exit rescan*/
						conflict = TRUE;
						break;
					}
				}
				/*If there's no conflict then try to issue*/
				if (conflict != TRUE){
					issue = issue_fu_mem(state->fu_mem_list, state->CQ[pointer].instr,  );
					/*Successful Issue*/
					if (issue == 0){
						/*Set issued bit to TRUE*/
						state->CQ[pointer].issued == TRUE;
					}
					/*Unsuccessful Issue*/
					else break;
				}
			}
		}
		/*If we've scanned from head to tail then exit for loop*/
		if (pointer == state->CQ_tail) break;
		/*Else keep scanning IQ*/
		else pointer = (pointer + 1) % CQ_SIZE;
	}

	/*Remove any issued instructions from CQ*/
	for (;;){
		if (state->CQ[state->CQ_head].issued == FALSE) break;
		else state->CQ_head = (state->CQ_head + 1) % IQ_SIZE;
	}
}


int
issue(state_t *state) {

	int pointer, remove, use_imm; 
	int issue = -1;
	operand_t result;
	const op_info_t *op_info;

	/*
	Scan IQ starting at head and issue a ready instruction
	Perform operation in this stage as well
	At the end of the stage remove and issued instructions from the head
	-Set pointer to head
	-For TRUE
		-If issued == FALSE
			-if tag1 && tag2 == TRUE
				-Perform operation
				-Try to issue the instruction
				-If succesfull issue
					-Set issue bit to TRUE
					-Write result to ROB
					-break
		-If pointer == tail
			-break
		-Else
			-Increment the pointer
	-Set pointer to head
	-Remove = TRUE
	-For remove == TRUE
		-If issued == FALSE
			remove = FALSE
		-Else
			-Increment head
			-Increment pointer

	*/
	
	pointer = state->IQ_head;
	
	for (;;) {
		if (state->IQ[pointer].issued == FALSE){
			if ((state->IQ[pointer].tag1 == -1) && (state->IQ[pointer].tag2 ==-1)){
				/*Decode Instruction*/
				op_info = decode_instr(state->IQ[pointer].instr, &use_imm);
				/*perform operation*/
				result = perform_operation(state->IQ[pointer].instr, state->IQ[pointer].pc, state->IQ[pointer].operand1, state->IQ[pointer].operand2);
				/*Issue Instruction*/
				if ((op_info->fu_group_num == FU_GROUP_INT)||(op_info->fu_group_num ==FU_GROUP_BRANCH)||(op_info->fu_group_num == FU_GROUP_MEM)) {
					issue = issue_fu_int(state->fu_int_list, instr, result);
				}
				if (op_info->fu_group_num == FU_GROUP_ADD) {
					issue = issue_fu_fp(state->fu_add_list, instr, result);
				}
				if (op_info->fu_group_num == FU_GROUP_MULT) {
					issue = issue_fu_fp(state->fu_mult_list, instr, result);
				}
				if(op_info->fu_group_num == FU_GROUP_DIV) {
					issue = issue_fu_fp(state->fu_div_list, instr, result);
				}
				/*Successful Issue*/
				if (issue == 0){
						/*Set issued bit to TRUE*/
					state->ROB[state->IQ[pointer].ROB_index].issued = TRUE;
						/*Write result to ROB*/
							/*Loads and Stores: Effective Address goes to target*/
					if (op_info->fu_group_num == FU_GROUP_MEM){
						state->ROB[state->IQ[pointer].ROB_index].target = result;
					}
							/*Branch; Address goes to target and Outcome goes to result*/
							/*This only accounts for J and BEQZ*/
					if (op_info->fu_group_num ==FU_GROUP_BRANCH){
						state->ROB[state->IQ[pointer].ROB_index].target = result;
								/*If the first operand is 0 then branch, indicated by 1*/
						if (state->IQ[pointer].operand1 ==  0) {
							state->ROB[state->IQ[pointer].ROB_index].result.integer.w = 1;
						}
								/*Otherwise don't branch, indicated by 0*/
						else state->ROB[state->IQ[pointer].ROB_index].result.integer.w = 0;
					}
							/*Otherwise goes to result*/
					else state->ROB[state->IQ[pointer].ROB_index].result = result;
						/*Exit for loop*/
					break;
				}
			}
		}
		/*If we've scanned from head to tail then exit for loop*/
		if (pointer == state->IQ_tail) break;
		/*Else keep scanning IQ*/
		else pointer = (pointer + 1) % IQ_SIZE;
	}
	/*Remove any issued instructions from IQ*/
	remove = TRUE;
	for (i = 0;remove == TRUE; i++){
		pointer = state->IQ_head;
		if (state->IQ[pointer].issued == FALSE) remove = FALSE;
		else {
			state->IQ_head = (state->IQ_head + 1) % IQ_SIZE;
		}
	}
}


int
dispatch(state_t *state) {

	/*Return -1 for stall, 1 for halt or NOP, 0 for issue*/

	int instr, use_imm, tag_check, full; /*Holds instruction, immediate flag, check tags, check queue*/
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
	-If ROB is full
		-stall
	-Insert instructions onto ROB at tail
	-Set completed field to FALSE (except for HALT = TRUE)
	-Increment ROB_tail (moved to end)
	*/
	
		/*Check if ROB is full*/
	full = check_queue(state, ROB);
	if (full == TRUE){
		stall(state);
		return -1;
	}

	state->ROB[state->ROB_tail].instr = instr;
	if (op_info->fu_group_num == FU_GROUP_HALT) state->ROB[state->ROB_tail].completed = TRUE;
	else state->ROB[state->ROB_tail].completed = FALSE;

	/*
	IQ
	-If IQ is full
		-stall
	-Insert instruction and pc onto IQ at tail (except HALT)
	-Set issued field to FALSE
	-Increment IQ_tail (moved to end)
	-Set ROB_index field
	-Set tag1/operand1 and tag2/operand2 fields
		-tag = -1 if value is ready and present
		-tag = ROB_index of in-flight instruction if value is being computed
	*/

		/*Check if IQ is full*/
	full = check_queue(state, IQ);
	if (full == TRUE){
		stall(state);
		return -1;
	}

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
		Set tag1/operand1 and tag2/operand2 fields.
		Check_in_flight_status takes the state, register and int/flt
		and returns the value in the 
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
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == TRUE){
				state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
			}
			state->IQ[state->IQ_tail].tag1 = tag_check;
		}
				/*Check R2 tag if no immediate*/
		if(use_imm == FALSE) {
			tag_check = check_in_flight_status(state);
			if (tag_check == -1){
				state->IQ[state->IQ_tail].operand2.integer.w = state->rf_int.reg_int.integer[r2].w;
				state->IQ[state->IQ_tail].tag2 = -1
			}
			else{
				tag_check = state->ROB[tag_check].completed;
				if (tag_check == TRUE){
					state->IQ[state->IQ_tail].operand2.integer.w = state->rf_int.reg_int.integer[r2].w;
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

			/*Check R1 tag*/
		tag_check = check_in_flight_status()
		if(tag_check == -1){
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == TRUE){
				state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
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
			state->IQ[state->IQ_tail].operand1.flt = state->rf_fp.reg_fp.flt[r1];
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == TRUE){
				state->IQ[state->IQ_tail].operand1.flt = state->rf_fp.reg_fp.flt[r1];
			}
			state->IQ[state->IQ_tail].tag1 = tag_check;
		}
				/*Check R2 tag*/
		tag_check = check_in_flight_status();
		if(tag_check == -1) {
			state->IQ[state->IQ_tail].operand2.flt = state->rf_fp.reg_fp.flt[r2];
			state->IQ[state->IQ_tail].tag2 = -1;
		}
		else{
			tag_check = state->ROB[tag_check].completed;
			if (tag_check == TRUE){
				state->IQ[state->IQ_tail].operand2.flt = state->rf_fp.reg_fp.flt[r2];
			}
			state->IQ[state->IQ_tail].tag2 = tag_check;
		}
				/*Set destination register R3 tag to ROB_index*/
		state->rf_fp.tag[r3] = state->ROB_tail;
	}

			/*Control*/
	if (op_info->fu_group_num == FU_GROUP_BRANCH){
				/*Set fetch_lock*/
		state->fetch_lock = TRUE;
				/*These instructions don't have a source register*/
		if ((op_info->operation == OPERATION_J)||(op_info->operation == OPERATION_JAL)) {
			state->IQ[state->IQ_tail].operand1.integer.w = 0;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
				/*Check R1 tag*/
		else{
			tag_check = check_in_flight_status();
			if(tag_check == -1) {
				state->IQ[state->IQ_tail].operand1.integer.wu = state->rf_int.reg_int.integer[r1].wu;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else{
				tag_check = state->ROB[tag_check].completed;
				if (tag_check == TRUE){
					state->IQ[state->IQ_tail].operand1.integer.wu = state->rf_int.reg_int.integer[r1].wu;
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

	/*
	CQ
	Only for Loads and Stores: memory access operation
	-If CQ is full
		-stall
	-Insert instructions onto CQ at tail
	-Set issued field to FALSE
	-Set ROB_index
	-If store 
		-Set store field to TRUE
	-If load
		-Set store field to FALSE
		-Ignore operand2
		-Set tag2 = -1	
	-Increment CQ_tail
	*/

	if (op_info->fu_group_num == FU_GROUP_MEM){

			/*Check if CQ is full*/
		full = check_queue(state, CQ);
		if (full == TRUE){
			stall(state);
			return -1;
		}

			/*Insert instruction onto CQ*/
		state->CQ[state->CQ_tail].instr = instr;

			/*Set Issued field to FALSE*/
		state->CQ[state->CQ_tail].issued = FALSE;

			/*Set ROB_index*/
		state->CQ[stateCQ_tail].ROB_index = state->ROB_tail;

			/*Link IQ computation to CQ tag1 + ROB_SIZE*/
		state->CQ[state->CQ_tail].tag1 = state->IQ_tail + ROB_SIZE;


			/*Handle Stores*/
		if (op_info->operation == OPERATION_STORE) {
			state->CQ[state->CQ_tail].store = TRUE;

				/*Check second source register R2 tag*/
				
						/*For FP*/
			if (op_info->data_type == DATA_TYPE_F){
				tag_check = check_in_flight_status()
				if(tag_check == -1){
					state->IQ[state->CQ_tail].operand1.integer.w = state->rf_fp.reg_fp.flt[r2];
					state->IQ[state->CQ_tail].tag2 = -1;
				}
				else{
					tag_check = state->IQ[tag_check].completed;
					if (tag_check == TRUE){
						state->CQ[state->CQ_tail].operand1.integer.w = state->rf_fp.reg_fp.flt[r2];
					}
					state->CQ[state->CQ_tail].tag2 = tag_check;
				}
			}

					/*For INT*/
			if (op_info->data_type == DATA_TYPE_W){
				tag_check = check_in_flight_status()
				if(tag_check == -1){
					state->CQ[state->CQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r2].w;
					state->CQ[state->CQ_tail].tag2 = -1;
				}
				else{
					tag_check = state->IQ[tag_check].completed;
					if (tag_check == TRUE){
						state->CQ[state->CQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r2].w;
					}
					state->CQ[state->CQ_tail].tag2 = tag_check;
				}
			}
		}

			/*Handle Loads*/
		if (op_info->operation == OPERATION_LOAD) {
			state->CQ[state->CQ_tail].store = FALSE;
			state->CQ[state->CQ_tail].tag2 = -1;
		}

			/*Increment CQ tail*/
		incr_tail(state, CQ);
	}	
	
	/*Increment ROB and IQ tails*/
	incr_tail(state, ROB);
	incr_tail(state, IQ);

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

/*
Added
*/

/*Handles stalls*/
void stall(state_t *state){
	/*Decrement the PC*/
	state->pc -= 4;
}

/*Function to increment the tail*/
void
incr_tail(state_t *state, int queue) {

	/*ROB*/
	if (queue == ROB){
		state->ROB_tail = (state->ROB_tail + 1) % ROB_SIZE;
	}

	/*IQ*/
	if (queue == IQ){
		state->IQ_tail = (state->IQ_tail + 1) % IQ_SIZE;
	}
	
	/*CQ*/
	if (queue == CQ){
		state->CQ_tail = (state->CQ_tail + 1) % CQ_SIZE;
	}
}
