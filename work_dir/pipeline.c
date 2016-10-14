
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
	printf("Commit\n");
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

	int instr, use_imm, issued, r1, r2, r3;
	operand_t result, target;
	const op_info_t *op_info;
	
	if (state->ROB[state->ROB_head].completed == TRUE) {

		instr = state->ROB[state->ROB_head].instr;
		result = state->ROB[state->ROB_head].result;
		target = state->ROB[state->ROB_head].target;
		op_info = decode_instr(instr, &use_imm);

		/*Perform Register macros for later use*/
		r1 = FIELD_R1(instr);
		r2 = FIELD_R2(instr);
		r3 = FIELD_R3(instr);

		/*HALT Condition*/
		if (op_info->fu_group_num == FU_GROUP_HALT) {
			*num_insn += 1;
			return TRUE;
		}

			/*Floating Point Commit*/
		if (op_info->data_type == DATA_TYPE_F) {
				/*FP Memory*/
			if (op_info->fu_group_num == FU_GROUP_MEM) {
					/*Loads*/
				if (op_info->operation == OPERATION_LOAD) {
					state->rf_fp.reg_fp.flt[r2] = result.flt;
					/*Set ready tag*/
					if (state->ROB_head == state->rf_fp.tag[r2]) state->rf_fp.tag[r2] = -1;
					*num_insn += 1;
				}
					/*Stores*/
				if (op_info->operation == OPERATION_STORE) {
					printf("Commit 67\n");
					issued = issue_fu_mem(state->fu_mem_list, state->ROB_head, TRUE, TRUE);
						/*Succesful Issue -> Copy to Memory*/
					if (issued == 0){
						printf("C float %f\n", result.flt);
						printf("C int %d\n", result.integer.w);
						printf("C hex %x\n", result.integer.w);
						state->mem[target.integer.w] = (result.integer.w >> 24) & 0xFF;
						state->mem[target.integer.w + 1] = (result.integer.w >> 16) & 0xFF;
						state->mem[target.integer.w + 2] = (result.integer.w >> 8) & 0xFF;
						state->mem[target.integer.w + 3] = result.integer.w & 0xFF;
						*num_insn += 1;
					}
					else return FALSE;
				}
			}
				/*FP Arithmetic*/
			else {
				state->rf_fp.reg_fp.flt[r3] = result.flt;
					/*Set ready tag*/
				if (state->ROB_head == state->rf_fp.tag[r3]) state->rf_fp.tag[r3] = -1;
				*num_insn += 1;
			}
		}

		/*Integer Commit*/
		/*INT Loads and Stores*/
		if (op_info->data_type == DATA_TYPE_W) {
			/*Loads*/
			if (op_info->operation == OPERATION_LOAD){
				state->rf_int.reg_int.integer[r2].w = result.integer.w;
				/*Set ready tag*/
				if (state->ROB_head == state->rf_int.tag[r2]) state->rf_int.tag[r2] = -1;
				*num_insn += 1;
			}
			/*Stores*/
			if (op_info->operation == OPERATION_STORE) {
				issued = issue_fu_mem(state->fu_mem_list, state->ROB_head, FALSE, TRUE);
				/*Succesful Issue -> Copy to Memory*/
				if (issued == 0){
					printf("INT store\n");
					state->mem[target.integer.w + 3] = result.integer.w;
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

		/*Control Instructions*/
		if (op_info->fu_group_num == FU_GROUP_BRANCH) *num_insn += 1;

		/*Increment ROB_head*/
		state->ROB_head = (state->ROB_head + 1) % ROB_SIZE;

		return FALSE;
	}
	
	else return FALSE;
}


void
writeback(state_t *state) {
	printf("writeback\n");

	int i, index;

	/*Integer*/
	for (i = 0; i < state->wb_port_int_num; i++) {
    	if (state->wb_port_int[i].tag != -1) {
      		/*Completed for all non effective address operations*/
      		if (state->wb_port_int[i].tag < ROB_SIZE){
      			state->ROB[state->wb_port_int[i].tag].completed = TRUE;
      		}
  			/*Check tag against IQ*/
  			for (index = state->IQ_head; index != state->IQ_tail; index = (index + 1) & (IQ_SIZE-1)) {
  				/*Tag 1 Match -> copy result to operand field and set tag = -1*/
  				if (state->IQ[index].tag1 == (state->wb_port_int[i].tag)) {
  					state->IQ[index].operand1 = state->ROB[state->wb_port_int[i].tag].result;
  					state->IQ[index].tag1 = -1;
  				}
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->IQ[index].tag2 == (state->wb_port_int[i].tag)) {
  					state->IQ[index].operand2 = state->ROB[state->wb_port_int[i].tag].result;
  					state->IQ[index].tag2 = -1;
  				}
  			}
  			/*Check tag against CQ*/
  			for (index = state->CQ_head; index != state->CQ_tail; index = (index + 1) & (CQ_SIZE-1)) {
  				/*Tag 1 Match -> copy result to operand field and set tag = -1*/
  				if (state->CQ[index].tag1 == state->wb_port_int[i].tag) {
  					state->CQ[index].address = state->ROB[state->wb_port_int[i].tag - ROB_SIZE].target;
  					state->CQ[index].tag1 = -1;
  				}
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->CQ[index].tag2 == state->wb_port_int[i].tag) {
  					state->CQ[index].result = state->ROB[state->wb_port_int[i].tag].result;
  					state->CQ[index].tag2 = -1;
  				}
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
  					state->IQ[index].operand1 = state->ROB[state->wb_port_fp[i].tag].result;
  					state->IQ[index].tag1 = -1;
  				}
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->IQ[index].tag2 == state->wb_port_fp[i].tag) {
  					state->IQ[index].operand2 = state->ROB[state->wb_port_fp[i].tag].result;
  					state->IQ[index].tag2 = -1;
  				}
  			}
  			/*Check tag against CQ*/
  			for (index = state->CQ_head; index != state->CQ_tail; index = (index + 1) & (CQ_SIZE-1)) {
  				/*Tag 1 should never match*/
  				/*Tag 2 Match -> copy result to operand field and set tag = -1*/
  				if (state->CQ[index].tag2 == state->wb_port_fp[i].tag) {
  					state->CQ[index].result.integer.w = state->ROB[state->wb_port_fp[i].tag].result.integer.w;
  					state->CQ[index].tag2 = -1;
  					printf("WB float %f\n", state->CQ[index].result.flt);
					printf("WB int %d\n", state->CQ[index].result.integer.w);
					printf("WB hex 0x%.8x\n", state->CQ[index].result.integer.w);
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
	printf("execute\n");
	advance_fu_mem(state->fu_mem_list, state->wb_port_int, state->wb_port_int_num, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_int(state->fu_int_list, state->wb_port_int, state->wb_port_int_num, &state->branch_tag);
	advance_fu_fp(state->fu_add_list, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_fp(state->fu_mult_list, state->wb_port_fp, state->wb_port_fp_num);
	advance_fu_fp(state->fu_div_list, state->wb_port_fp, state->wb_port_fp_num);
}


int
memory_disambiguation(state_t *state) {
	printf("MD\n");
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

	int use_imm;
	op_info_t *op_info, *op_info_rescan;

	int pointer, rescan, conflict, issued, match;

	pointer = state->CQ_head;
	
	for (;;) {
		conflict = FALSE;
		match = FALSE;
		
		/*Stores*/
		if (state->CQ[pointer].store == TRUE){
			/*No Effective Address*/
			if (state->CQ[pointer].tag1 != -1) {
				break;
			}
			/*Unissued and Ready*/
			if ((state->CQ[pointer].issued == FALSE) && (state->CQ[pointer].tag1 == -1) && (state->CQ[pointer].tag2 == -1)){
				state->CQ[pointer].issued = TRUE;
				state->ROB[state->CQ[pointer].ROB_index].completed = TRUE;
				state->ROB[state->CQ[pointer].ROB_index].result = state->CQ[pointer].result;
				break;
			}
		}
		/*Loads*/
		if (state->CQ[pointer].store == FALSE){
			/*Unissued and Ready*/
			if ((state->CQ[pointer].issued == FALSE) && (state->CQ[pointer].tag1 == -1)){
				printf("MD line 294\n");
				/*Check for conflicts with an older store*/
				for (rescan = state->CQ_head; !(rescan == pointer); rescan = (rescan + 1) % CQ_SIZE ){
					printf("MD line 297\n");
					/*If an unissued store is writing the same location*/
					if ((state->CQ[rescan].store == TRUE) && (state->CQ[rescan].issued == FALSE) && (state->CQ[rescan].address.integer.w == state->CQ[pointer].address.integer.w)) {
						/*Set conflict flag and exit rescan*/
						printf("MD line 301\n");
						conflict = TRUE;
						break;
					}
				}
				/*If there's no conflict then try to issue*/
				if (conflict == FALSE){
					printf("MD line 308\n");
					op_info = decode_instr(state->CQ[pointer].instr, &use_imm);
					/*FP Loads*/
					if (op_info->data_type == DATA_TYPE_F) {
						printf("MD line 315\n");
						issued = issue_fu_mem(state->fu_mem_list, state->CQ[pointer].ROB_index, TRUE, FALSE);
						/*Successful Issue*/
						if (issued == 0){
							printf("MD line 319\n");
							state->CQ[pointer].issued = TRUE;
							/*state->ROB[state->CQ[pointer].ROB_index].completed = TRUE;*/
							/*Check completed Stores in ROB for same effective address*/
							for (rescan = state->ROB_head; !(rescan == state->ROB_tail + 1); rescan = (rescan + 1) % ROB_SIZE) {
								op_info_rescan = decode_instr(state->ROB[rescan].instr, &use_imm);
								if ((state->ROB[rescan].target.integer.w == state->ROB[state->CQ[pointer].ROB_index].target.integer.w)&&
								(state->ROB[rescan].completed ==TRUE)&&(op_info_rescan->operation == OPERATION_STORE)) {
									state->ROB[state->CQ[pointer].ROB_index].result = state->ROB[rescan].result;
									match = TRUE;
								}
							}
							/*Else load from memory*/
							if (match == FALSE){
								union IntFloat val;
								val.i = ((state->mem[state->ROB[state->CQ[pointer].ROB_index].target.integer.w]<<24)|
									(state->mem[state->ROB[state->CQ[pointer].ROB_index].target.integer.w+1]<<16)|
									(state->mem[state->ROB[state->CQ[pointer].ROB_index].target.integer.w+2]<<8)|
									(state->mem[state->ROB[state->CQ[pointer].ROB_index].target.integer.w+3]));
								state->ROB[state->CQ[pointer].ROB_index].result.flt = val.f;
							}	
						}
						/*Unsuccessful Issue*/
						else break;
					}
					/*INT Loads*/
					if (op_info->data_type == DATA_TYPE_W) {
						printf("MD line 341\n");
						issued = issue_fu_mem(state->fu_mem_list, state->CQ[pointer].ROB_index, FALSE, FALSE);
						/*Successful Issue*/
						if (issued == 0){
							printf("MD line 345\n");
							state->CQ[pointer].issued = TRUE;
							/*state->ROB[state->CQ[pointer].ROB_index].completed = TRUE;*/
							/*Check completed Stores in ROB for same effective address*/
							for (rescan = state->ROB_head; !(rescan == state->ROB_tail + 1); rescan = (rescan + 1) % ROB_SIZE) {
								op_info_rescan = decode_instr(state->ROB[rescan].instr, &use_imm);
								if ((state->ROB[rescan].target.integer.w == state->ROB[state->CQ[pointer].ROB_index].target.integer.w)&&
								(state->ROB[rescan].completed ==TRUE)&&(op_info->operation == OPERATION_STORE)) {
									state->ROB[state->CQ[pointer].ROB_index].result = state->ROB[rescan].result;
									match = TRUE;
								}
							}
							/*Else load from memory*/
							if (match == FALSE){
								state->ROB[state->CQ[pointer].ROB_index].result.integer.w = state->mem[state->ROB[state->CQ[pointer].ROB_index].target.integer.w + 3];
							}
						/*Unsuccessful Issue*/
						else break;
						}
					}
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
		state->CQ_head = (state->CQ_head + 1) % IQ_SIZE;
	}
}


int issue(state_t *state) {
	printf("Issue\n");

	int pointer, use_imm; 
	int issued = -1;
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
				if (op_info->fu_group_num == FU_GROUP_INT) {
					issued = issue_fu_int(state->fu_int_list, state->IQ[pointer].ROB_index, FALSE, FALSE);
				}
				if (op_info->fu_group_num == FU_GROUP_MEM){
					issued = issue_fu_int(state->fu_int_list, state->IQ[pointer].ROB_index + ROB_SIZE, FALSE, FALSE);
				}
				if (op_info->fu_group_num ==FU_GROUP_BRANCH) {
					issued = issue_fu_int(state->fu_int_list, state->IQ[pointer].ROB_index, TRUE, FALSE);
				}
				if (op_info->fu_group_num == FU_GROUP_ADD) {
					issued = issue_fu_fp(state->fu_add_list, state->IQ[pointer].ROB_index);
				}
				if (op_info->fu_group_num == FU_GROUP_MULT) {
					issued = issue_fu_fp(state->fu_mult_list, state->IQ[pointer].ROB_index);
				}
				if(op_info->fu_group_num == FU_GROUP_DIV) {
					issued = issue_fu_fp(state->fu_div_list, state->IQ[pointer].ROB_index);
				}
				/*Successful Issue*/
				if (issued == 0){
						/*Set issued bit to TRUE*/
					state->IQ[pointer].issued = TRUE;
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
						if (state->IQ[pointer].operand1.integer.w ==  0) {
							state->ROB[state->IQ[pointer].ROB_index].result.integer.w = 1;
						}
								/*Otherwise don't branch, indicated by 0*/
						else state->ROB[state->IQ[pointer].ROB_index].result.integer.w = 0;
					}
							/*All others go to result*/
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
	printf("Issue 488\n");
	for (pointer = state->IQ_head; pointer != state->IQ_tail; pointer = (pointer + 1) & (IQ_SIZE-1)){
		printf("Issue 490\n");
		if (state->IQ[pointer].issued == FALSE) break;
		state->IQ_head = (state->IQ_head + 1) % IQ_SIZE;
	}
	return 0;
}


int
dispatch(state_t *state) {
	printf("dispatch\n");

	/*Return -1 for stall, 1 for halt or NOP, 0 for issue*/

	int instr, use_imm, tag_check, full, completed; /*Holds instruction, immediate flag, check tags, check queue*/
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
		return 1;
	}

	/*Stall if any queue is full*/
	if ((state->ROB_head == (state->ROB_tail + 1) % ROB_SIZE)||
		(state->IQ_head == (state->IQ_tail + 1) % IQ_SIZE)||
		(state->CQ_head == (state->CQ_tail + 1) % CQ_SIZE)) {
		stall(state);
		return -1;
	}

	/*
	ROB
	-Insert instructions onto ROB at tail
	-Set completed field to FALSE 
	- Manage HALT
	*/
	
	state->ROB[state->ROB_tail].instr = instr;
	if (op_info->fu_group_num == FU_GROUP_HALT) {
		state->ROB[state->ROB_tail].completed = TRUE;
		incr_tail(state, _ROB_);
		state->fetch_lock = TRUE;
		return 1;
	}
	else state->ROB[state->ROB_tail].completed = FALSE;

	/*
	IQ
	-Insert instruction and pc onto IQ at tail
	-Set issued field to FALSE
	-Increment IQ_tail (moved to end)
	-Set ROB_index field
	-Set tag1/operand1 and tag2/operand2 fields
		-tag = -1 if value is ready and present
		-tag = ROB_index of in-flight instruction if value is being computed
	*/

		/*Insert instruction and pc onto IQ*/
	state->IQ[state->IQ_tail].instr = instr;
	state->IQ[state->IQ_tail].pc = state->pc;

		/*Issued field to FALSE*/
	state->IQ[state->IQ_tail].issued = FALSE;

		/*Set ROB_index field*/
	if (op_info->fu_group_num == FU_GROUP_MEM){
		state->IQ[state->IQ_tail].ROB_index = state->ROB_tail /*+ ROB_SIZE*/;
	}
	else state->IQ[state->IQ_tail].ROB_index = state->ROB_tail;

		/*
		Set tag1/operand1 and tag2/operand2 fields.
		Check_in_flight_status takes the state, register and int/flt
		and returns the value in the register file tag
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
				/*Check R1 register file tag*/
		tag_check = check_in_flight_status(state, r1, 0);
		if(tag_check == -1){
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			completed = state->ROB[tag_check].completed;
			if (completed == TRUE){
				state->IQ[state->IQ_tail].operand1 = state->ROB[tag_check].result;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else{
			state->IQ[state->IQ_tail].tag1 = tag_check;
			}
		}
				/*Check R2 register file tag if no immediate*/
		if(use_imm == FALSE) {
			tag_check = check_in_flight_status(state, r2, 0);
			if(tag_check == -1){
				state->IQ[state->IQ_tail].operand2.integer.w = state->rf_int.reg_int.integer[r1].w;
				state->IQ[state->IQ_tail].tag2 = -1;
			}
			else{
				completed = state->ROB[tag_check].completed;
				if (completed == TRUE){
					state->IQ[state->IQ_tail].operand2 = state->ROB[tag_check].result;
					state->IQ[state->IQ_tail].tag2 = -1;
				}
				else{
				state->IQ[state->IQ_tail].tag2 = tag_check;
				}
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
		tag_check = check_in_flight_status(state, r1, 0);
		if(tag_check == -1){
			state->IQ[state->IQ_tail].operand1.integer.w = state->rf_int.reg_int.integer[r1].w;
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			completed = state->ROB[tag_check].completed;
			if (completed == TRUE){
				state->IQ[state->IQ_tail].operand1 = state->ROB[tag_check].result;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else {
				state->IQ[state->IQ_tail].tag1 = tag_check;
			}
		}

			/*Second operand is an immediate and always ready*/
		state->IQ[state->IQ_tail].operand2.integer.w = imm;
		state->IQ[state->IQ_tail].tag2 = -1;
			
			/*Set destination register tag to ROB_index -> Only loads*/
		if (op_info->operation == OPERATION_LOAD)
			if (op_info->data_type == DATA_TYPE_F){
				state->rf_fp.tag[r2] = state->ROB_tail;
			}
			else {
				state->rf_int.tag[r2] = state->ROB_tail;
			}
	}

			/*Floating Point Arithmetic*/
	if ((op_info->fu_group_num == FU_GROUP_ADD)||(op_info->fu_group_num == FU_GROUP_MULT)||(op_info->fu_group_num == FU_GROUP_DIV)){
				/*Check R1 register file tag*/
		tag_check = check_in_flight_status(state, r1, 1);
		if(tag_check == -1) {
			state->IQ[state->IQ_tail].operand1.flt = state->rf_fp.reg_fp.flt[r1];
			state->IQ[state->IQ_tail].tag1 = -1;
		}
		else{
			completed = state->ROB[tag_check].completed;
			if (completed == TRUE){
				state->IQ[state->IQ_tail].operand1.flt = state->ROB[tag_check].result.flt;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else {
				state->IQ[state->IQ_tail].tag1 = tag_check;
			}
		}
				/*Check R2 register file tag*/
		tag_check = check_in_flight_status(state, r2, 1);
		if(tag_check == -1) {
			state->IQ[state->IQ_tail].operand2.flt = state->rf_fp.reg_fp.flt[r2];
			state->IQ[state->IQ_tail].tag2 = -1;
		}
		else{
			completed = state->ROB[tag_check].completed;
			if (completed == TRUE){
				state->IQ[state->IQ_tail].operand2.flt = state->ROB[tag_check].result.flt;
				state->IQ[state->IQ_tail].tag2 = -1;
			}
			else{
				state->IQ[state->IQ_tail].tag2 = tag_check;
			}
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
				/*Check R1 register file tag*/
		else{
			tag_check = check_in_flight_status(state, r1, 0);
			if(tag_check == -1) {
				state->IQ[state->IQ_tail].operand1.integer.wu = state->rf_int.reg_int.integer[r1].wu;
				state->IQ[state->IQ_tail].tag1 = -1;
			}
			else{
				completed = state->ROB[tag_check].completed;
				if (completed == TRUE){
					state->IQ[state->IQ_tail].operand1.integer.wu = state->ROB[tag_check].result.integer.wu;
					state->IQ[state->IQ_tail].tag1 = -1;
				}
				else {
				state->IQ[state->IQ_tail].tag1 = tag_check;
				}
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

			/*Insert instruction onto CQ*/
		state->CQ[state->CQ_tail].instr = instr;

			/*Set Issued field to FALSE*/
		state->CQ[state->CQ_tail].issued = FALSE;

			/*Set ROB_index*/
		state->CQ[state->CQ_tail].ROB_index = state->ROB_tail;

			/*Link IQ computation to CQ tag1*/
		state->CQ[state->CQ_tail].tag1 = state->ROB_tail + ROB_SIZE;


			/*Handle Stores*/
		if (op_info->operation == OPERATION_STORE) {
			state->CQ[state->CQ_tail].store = TRUE;

				/*Check second source register R2 tag*/
				
						/*For FP*/
			if (op_info->data_type == DATA_TYPE_F){
				tag_check = check_in_flight_status(state, r2, 1);
				if(tag_check == -1){
					state->CQ[state->CQ_tail].result.flt = state->rf_fp.reg_fp.flt[r2];
					state->CQ[state->CQ_tail].tag2 = -1;
				}
				else{
					completed = state->ROB[tag_check].completed;
					if (completed == TRUE){
						state->CQ[state->CQ_tail].result = state->ROB[tag_check].result;
						state->CQ[state->CQ_tail].tag2 = -1;
					}
					else state->CQ[state->CQ_tail].tag2 = tag_check;
				}
			}

					/*For INT*/
			if (op_info->data_type == DATA_TYPE_W){
				tag_check = check_in_flight_status(state, r2, 0);
				if(tag_check == -1){
					state->CQ[state->CQ_tail].result.integer.w = state->rf_int.reg_int.integer[r2].w;
					state->CQ[state->CQ_tail].tag2 = -1;
				}
				else{
					completed = state->ROB[tag_check].completed;
					if (completed == TRUE){
						state->CQ[state->CQ_tail].result.integer.w = state->rf_int.reg_int.integer[r2].w;
						state->CQ[state->CQ_tail].tag2 = -1;
					}
					else state->CQ[state->CQ_tail].tag2 = tag_check;
				}
			}
		}

			/*Handle Loads*/
		if (op_info->operation == OPERATION_LOAD) {
			state->CQ[state->CQ_tail].store = FALSE;
			state->CQ[state->CQ_tail].tag2 = -1;
		}

			/*Increment CQ tail*/
		incr_tail(state, _CQ_);
	}	
	
	/*Increment ROB and IQ tails*/
	incr_tail(state, _ROB_);
	incr_tail(state, _IQ_);

	/*Ensure that R0 tag is always -1*/
	state->rf_int.tag[0] = -1;
}

void
fetch(state_t *state) {
	printf("fetch\n");

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
void incr_tail(state_t *state, int queue) {

	/*ROB*/
	if (queue == _ROB_){
		state->ROB_tail = (state->ROB_tail + 1) % ROB_SIZE;
	}

	/*IQ*/
	if (queue == _IQ_){
		state->IQ_tail = (state->IQ_tail + 1) % IQ_SIZE;
	}
	
	/*CQ*/
	if (queue == _CQ_){
		state->CQ_tail = (state->CQ_tail + 1) % CQ_SIZE;
	}
}

/*Pulls INT or FP register file tag*/
int check_in_flight_status( state_t *state, unsigned int reg, int type) {
	int tag_check;
	/*Type 0 for INT, type 1 for FP*/
	if (type == 0){
		tag_check = state->rf_int.tag[reg];
	}
	if (type == 1){
		tag_check = state->rf_fp.tag[reg];
	}
	return tag_check;
}
