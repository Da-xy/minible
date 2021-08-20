/* 
 * This file is part of the Mooltipass Project (https://github.com/mooltipass).
 * Copyright (c) 2019 Stephan Mathieu
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/*!  \file     logic_database.h
*    \brief    General logic for database operations
*    Created:  17/03/2019
*    Author:   Mathieu Stephan
*/


#ifndef LOGIC_DATABASE_H_
#define LOGIC_DATABASE_H_

#include "comms_hid_msgs.h"
#include "nodemgmt.h"
#include "defines.h"


/* Prototypes */
RET_TYPE logic_database_add_webauthn_credential_for_service(uint16_t service_addr, uint8_t* user_handle, uint8_t user_handle_len, cust_char_t* user_name, cust_char_t* display_name, uint8_t* private_key,  uint8_t* ctr, uint8_t* credential_id, uint8_t keyType);
void logic_database_get_webauthn_data_for_address_and_inc_count(uint16_t child_addr, uint8_t* user_handle, uint8_t *user_handle_len, uint8_t* credential_id, uint8_t* key, uint32_t* count, uint8_t* ctr, uint8_t *keyType);
void logic_database_update_webauthn_credential(uint16_t child_address, cust_char_t* user_name, cust_char_t* display_name, uint8_t* private_key,  uint8_t* ctr, uint8_t* credential_id, uint8_t keyType);
RET_TYPE logic_database_add_child_node_to_data_service(uint16_t logic_user_data_service_addr, uint16_t* logic_user_last_data_child_addr, hid_message_store_data_into_file_t* store_data_request);
uint16_t logic_database_fill_get_cred_message_answer(uint16_t child_node_addr, hid_message_t* send_msg, uint8_t* cred_ctr, BOOL* prev_gen_credential_flag, BOOL* password_valid);
RET_TYPE logic_database_add_credential_for_service(uint16_t service_addr, cust_char_t* login, cust_char_t* desc, cust_char_t* third, uint8_t* password, uint8_t* ctr);
uint16_t logic_database_get_prev_2_fletters_services(uint16_t start_address, cust_char_t start_char, cust_char_t* char_array, uint16_t credential_type_id);
uint16_t logic_database_get_next_2_fletters_services(uint16_t start_address, cust_char_t cur_char, cust_char_t* char_array, uint16_t credential_type_id);
RET_TYPE logic_database_add_TOTP_credential_for_service(uint16_t service_addr, cust_char_t* login, TOTPcredentials_t const *TOTPcreds, uint8_t *ctr);
uint16_t logic_database_get_number_of_creds_for_service(uint16_t parent_addr, uint16_t* fnode_addr, uint16_t* lnode_used_addr, BOOL category_filter);
uint16_t logic_database_search_for_next_data_parent_after_addr(uint16_t node_addr, nodemgmt_data_category_te data_type, cust_char_t* service_name);
void logic_database_fetch_encrypted_password(uint16_t child_node_addr, uint8_t* password, uint8_t* cred_ctr, BOOL* prev_gen_credential_flag);
void logic_database_fetch_encrypted_TOTPsecret(uint16_t child_node_addr, uint8_t* TOTPsecret, uint8_t *TOTPsecretLen, uint8_t* TOTP_ctr);
uint16_t logic_database_search_service(cust_char_t* name, service_compare_mode_te compare_type, BOOL cred_type, uint16_t category_id);
void logic_database_update_credential(uint16_t child_addr, cust_char_t* desc, cust_char_t* third, uint8_t* password, uint8_t* ctr);
uint16_t logic_database_search_webauthn_userhandle_in_service(uint16_t parent_addr, uint8_t* user_handle, uint8_t user_handle_len);
void logic_database_get_webauthn_userhandle_for_address(uint16_t child_addr, uint8_t* user_handle, uint8_t *user_handle_len);
RET_TYPE logic_database_update_TOTP_credentials(uint16_t child_addr, TOTPcredentials_t const *TOTPcreds, uint8_t* ctr);
uint16_t logic_database_add_service(cust_char_t* service, service_type_te cred_type, uint16_t data_category_id);
uint16_t logic_database_search_login_in_service(uint16_t parent_addr, cust_char_t* login, BOOL category_filter);
uint16_t logic_database_search_webauthn_credential_id_in_service(uint16_t parent_addr, uint8_t* credential_id);
void logic_database_get_webauthn_username_for_address(uint16_t child_addr, cust_char_t* user_name);
void logic_database_get_login_for_address(uint16_t child_addr, cust_char_t** login);

#endif /* LOGIC_DATABASE_H_ */
