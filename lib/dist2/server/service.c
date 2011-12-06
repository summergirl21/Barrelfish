#include <stdio.h>
#include <string.h>

#include <barrelfish/barrelfish.h>
#include <barrelfish/nameservice_client.h>
#include <skb/skb.h> // read list

#include <if/dist_defs.h>
#include <if/dist_event_defs.h>

#include <dist2_server/service.h>
#include <dist2_server/query.h>
#include <dist2_server/debug.h>

#include <dist2/parser/ast.h>

#include <dist2/getset.h>


#include "queue.h"

char* strdup(const char*);


static errval_t new_dist_reply_state(struct dist_reply_state** drt, dist_reply_handler_fn reply_handler)
{
    assert(*drt == NULL);
    *drt = malloc(sizeof(struct dist_reply_state));
    if(*drt == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    memset(*drt, 0, sizeof(struct dist_reply_state));
    (*drt)->rpc_reply = reply_handler;
    (*drt)->next = NULL;

    return SYS_ERR_OK;
}


static void free_dist_reply_state(void* arg) {
    if(arg != NULL) {
        struct dist_reply_state* drt = (struct dist_reply_state*) arg;
        free(drt);
    }
    else {
        assert(!"free_reply_state with NULL argument?");
    }
}


static void get_reply(struct dist_binding* b, struct dist_reply_state* srt) {
    errval_t err;
    err = b->tx_vtbl.get_response(b, MKCONT(free_dist_reply_state, srt),
    		                      srt->query_state.stdout.buffer, srt->query_state.stderr.buffer,
    		                      srt->error);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srt);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void get_handler(struct dist_binding *b, char *query)
{
	errval_t err = SYS_ERR_OK;

	struct dist_reply_state* srt = NULL;
	err = new_dist_reply_state(&srt, get_reply);
	assert(err_is_ok(err));

	struct ast_object* ast = NULL;
	err = generate_ast(query, &ast);
	if(err_is_ok(err)) {
	    DIST2_DEBUG("get handler: %s\n", query);
		err = get_record(ast, &srt->query_state);
	}

	srt->error = err;
	srt->rpc_reply(b, srt);

	free_ast(ast);
	free(query);
}


static void get_names_reply(struct dist_binding* b, struct dist_reply_state* srt) {
    errval_t err;
    err = b->tx_vtbl.get_names_response(b, MKCONT(free_dist_reply_state, srt),
                                        srt->query_state.stdout.buffer, srt->error);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
            dist_rpc_enqueue_reply(b, srt);
            return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void get_names_handler(struct dist_binding *b, char *query)
{
    DIST2_DEBUG(" get_names_handler: %s\n", query);

    errval_t err = SYS_ERR_OK;

    struct dist_reply_state* srt = NULL;
    err = new_dist_reply_state(&srt, get_names_reply);
    assert(err_is_ok(err));

    struct ast_object* ast = NULL;
    err = generate_ast(query, &ast);
    if(err_is_ok(err)) {
        err = get_record_names(ast, &srt->query_state);
    }

    srt->error = err;
    srt->rpc_reply(b, srt);

    free_ast(ast);
    free(query);
}

static void set_reply(struct dist_binding* b, struct dist_reply_state* srs)
{
    char* record = srs->return_record ? srs->query_state.stdout.buffer : NULL;

    errval_t err;
    err = b->tx_vtbl.set_response(b, MKCONT(free_dist_reply_state, srs),
                                  record, srs->error);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void set_handler(struct dist_binding *b, char *query, uint64_t mode, bool get)
{
    DIST2_DEBUG(" set_handler: %s\n", query);
	errval_t err = SYS_ERR_OK;

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, set_reply);
	assert(err_is_ok(err));

	struct ast_object* ast = NULL;
	err = generate_ast(query, &ast);
	if(err_is_ok(err)) {
		DIST2_DEBUG("set record: %s\n", query);
		err = set_record(ast, mode, &srs->query_state);
	}

	srs->error = err;
	srs->return_record = true;
	srs->rpc_reply(b, srs);

	free_ast(ast);
	free(query);
}


static void del_reply(struct dist_binding* b, struct dist_reply_state* srs)
{
    errval_t err;
    err = b->tx_vtbl.del_response(b, MKCONT(free_dist_reply_state, srs),
    		                      srs->error);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void del_handler(struct dist_binding* b, char* query)
{
    DIST2_DEBUG(" del_handler: %s\n", query);
	errval_t err = SYS_ERR_OK;

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, del_reply);
	assert(err_is_ok(err));

	struct ast_object* ast = NULL;
	err = generate_ast(query, &ast);
	if(err_is_ok(err)) {
		err = del_record(ast, &srs->query_state);
	}

	srs->error = err;
	srs->rpc_reply(b, srs);

	free_ast(ast);
	free(query);
}


static void watch_reply(struct dist_binding* b, struct dist_reply_state* drs)
{
    errval_t err;
    err = b->tx_vtbl.watch_response(b, MKCONT(free_dist_reply_state, drs),
                                    drs->client_id, drs->watch_id,
                                    drs->query_state.stdout.buffer,
                                    drs->error);

    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
            dist_rpc_enqueue_reply(b, drs);
            return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void watch_handler(struct dist_binding* b, char* query, uint64_t mode, dist_binding_type_t type, uint64_t client_id)
{
    errval_t err = SYS_ERR_OK;

    struct dist_reply_state* drs = NULL;
    struct dist_reply_state* drs_event = NULL;
    err = new_dist_reply_state(&drs, watch_reply);
    assert(err_is_ok(err));

    struct ast_object* ast = NULL;
    err = generate_ast(query, &ast);
    if(err_is_ok(err)) {
        switch(type) {
        case dist_BINDING_RPC:
            // TODO set on new_d_r_state()?
            drs->client_id = client_id;
            drs->binding = b;

            err = set_watch(ast, mode, drs);
            break;

        case dist_BINDING_EVENT:
            err = new_dist_reply_state(&drs_event, watch_reply);
            assert(err_is_ok(err));

            // TODO set on new_d_r_state()?
            drs_event->binding = b;
            drs_event->client_id = client_id;

            err = set_watch(ast, mode, drs_event);
            drs->error = err;
            drs->rpc_reply(b, drs);
            break;
        }
    }

    free_ast(ast);
    free(query);
}


static void exists_reply(struct dist_binding* b, struct dist_reply_state* drs)
{
    errval_t err;
    err = b->tx_vtbl.exists_response(b, MKCONT(free_dist_reply_state, drs),
                                     drs->query_state.stdout.buffer,
                                     drs->error);

    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
            dist_rpc_enqueue_reply(b, drs);
            return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void exists_handler(struct dist_binding* b, char* query, bool block)
{
    errval_t err = SYS_ERR_OK;

    struct dist_reply_state* drt = NULL;
    err = new_dist_reply_state(&drt, exists_reply);
    assert(err_is_ok(err));

    struct ast_object* ast = NULL;
    err = generate_ast(query, &ast);
    if(err_is_ok(err)) {
        DIST2_DEBUG("exists_handler get record\n");
        err = get_record(ast, &drt->query_state);
        if(err_is_ok(err) || !block) {
            // return immediately
            drt->error = err;
            drt->rpc_reply(b, drt);
        }
        if(err_no(err) == DIST2_ERR_NO_RECORD) {
            DIST2_DEBUG("exists_handler set watch\n");
            // register and wait until record available
            drt->client_id = 0;
            drt->type = dist_BINDING_RPC;
            drt->binding = b;

            err = set_watch(ast, DIST_ON_SET, drt);
            assert(err_is_ok(err)); // TODO
        }
    }

    free_ast(ast);
    free(query);
}


static void exists_not_reply(struct dist_binding* b, struct dist_reply_state* drs)
{
    errval_t err;
    err = b->tx_vtbl.exists_not_response(b, MKCONT(free_dist_reply_state, drs),
                                         drs->error);

    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
            dist_rpc_enqueue_reply(b, drs);
            return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void exists_not_handler(struct dist_binding* b, char* query, bool block) // TODO block arg
{
    assert(query != NULL);
    DIST2_DEBUG(" exists not handler: %s\n", query);

    errval_t err = SYS_ERR_OK;
    struct dist_reply_state* drt = NULL;
    err = new_dist_reply_state(&drt, exists_not_reply);
    assert(err_is_ok(err));

    struct ast_object* ast = NULL;
    err = generate_ast(query, &ast);
    if(err_is_ok(err)) {
        err = get_record(ast, &drt->query_state);
        if(err_is_ok(err)) {
            // register and wait until record unavailable
            DIST2_DEBUG(" exists not handler set watch: %s\n", query);

            drt->client_id = 0;
            drt->type = dist_BINDING_RPC;
            drt->binding = b;
            err = set_watch(ast, DIST_ON_DEL, drt);
            assert(err_is_ok(err)); // TODO
        }
        else if(err_no(err) == DIST2_ERR_NO_RECORD) {
            // return immediately
            drt->error = SYS_ERR_OK;
            drt->rpc_reply(b, drt);
        }
        else {
            DEBUG_ERR(err, "not_exists_handler unexpected error!");
            abort();
        }
    }

    free_ast(ast);
    free(query);
}


static void subscribe_reply(struct dist_binding* b, struct dist_reply_state* srs)
{
    errval_t err;
    err = b->tx_vtbl.subscribe_response(b, MKCONT(free_dist_reply_state, srs),
    		                            srs->error);

    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void subscribe_handler(struct dist_binding *b, char* query, uint64_t id)
{
	errval_t err = SYS_ERR_OK;

	DIST2_DEBUG("subscribe: query = %s\n", query);

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, subscribe_reply);
	assert(err_is_ok(err));

	struct ast_object* ast = NULL;
	err = generate_ast(query, &ast);
	if(err_is_ok(err)) {
		err = add_subscription(b, ast, id, &srs->query_state);
	}

	srs->error = err;
	srs->rpc_reply(b, srs);

	free_ast(ast);
	free(query);
}


static void unsubscribe_reply(struct dist_binding* b, struct dist_reply_state* srs) {
    errval_t err;
    err = b->tx_vtbl.unsubscribe_response(b, MKCONT(free_dist_reply_state, srs),
			                              srs->query_state.exec_res);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void unsubscribe_handler(struct dist_binding *b, uint64_t id)
{
	errval_t err = SYS_ERR_OK;

	DIST2_DEBUG("unsubscribe: id = %lu\n", id);

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, unsubscribe_reply);
	assert(err_is_ok(err));

	err = del_subscription(b, id, &srs->query_state);

	srs->error = err;
	srs->rpc_reply(b, srs);
}


static void send_subscribed_message(struct dist_event_binding* b,
		                            uint64_t id,
		                            char* object)
{
    errval_t err;
    err = b->tx_vtbl.subscribed_message(b, NOP_CONT, id, object);
}


static void publish_reply(struct dist_binding* b, struct dist_reply_state* srs) {
    errval_t err;
    err = b->tx_vtbl.publish_response(b, MKCONT(free_dist_reply_state, srs),
			                          srs->query_state.exec_res);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void publish_handler(struct dist_binding *b, char* object)
{
	DIST2_DEBUG("publish_handler\n");
	errval_t err = SYS_ERR_OK;

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, publish_reply);
	assert(err_is_ok(err));

	struct ast_object* ast = NULL;
	err = generate_ast(object, &ast);
	assert(err_is_ok(err));

	srs->rpc_reply(b, srs);

	DIST2_DEBUG("find_subscribers\n");
	err = find_subscribers(ast, &srs->query_state);
	if(err_is_ok(err)) {
        struct dist_event_binding* recipient = NULL;
        uint64_t id = 0;

    	DIST2_DEBUG("list_parser_status\n");

        // TODO remove list parser dependency
        struct list_parser_status status;
        skb_read_list_init_offset(&status, srs->query_state.stdout.buffer, 0);

        // Send to all subscribers
        while(skb_read_list(&status, "subscriber(%lu, %lu)", (uintptr_t*) &recipient, &id) ) {
            DIST2_DEBUG("publish msg to: recipient:%p id:%lu\n", recipient, id);
            send_subscribed_message(recipient, id, object);
        }
	}

	//free(object); TODO: used by send_subscribed_object
	free_ast(ast);
}

/*
static void lock_reply(struct dist_binding* b, struct dist_reply_state* srs) {
    errval_t err;
    err = b->tx_vtbl.lock_response(b, MKCONT(free_dist_reply_state, srs),
			                          srs->query_state.exec_res);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


static uint64_t enumerator = 0;

void lock_handler(struct dist_binding* b, char* query)
{
	assert(query != NULL); // todo
	errval_t err = SYS_ERR_OK;
	DIST2_DEBUG("lock handler: %s\n", query);


	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, lock_reply); // TODO FIX we dont always reply in this case we loose some srs!
	assert(err_is_ok(err)); // TODO

	struct ast_object* ast = NULL;
	err = generate_ast(query, &ast);
	assert(err_is_ok(err));

	err = get_record(ast, &srs->query_state);
	if(err_no(err) == DIST2_ERR_NO_RECORD)
	{
		DIST2_DEBUG("lock did not exist, create!\n");

		ast_append_attribute(ast, ast_pair(ast_ident(strdup("owner")), ast_num( (int64_t)b) )); // TODO strdup

		// Create new Lock
		// Overwrite err variable on purpose
		err = set_record(ast, &srs->query_state);

		srs->error = err;
		srs->rpc_reply(b, srs);
	}
	else if(err_is_ok(err)) {

		struct ast_object* ast2 = NULL;
		err = generate_ast(srs->query_state.stdout.buffer, &ast2);
		assert(err_is_ok(err)); // TODO

		struct ast_object* owner = ast_find_attribute(ast2, "owner");
		assert(owner != NULL);

		if( ((void*)owner->pn.right->cn.value) == b) { // TODO compare int64_t vs. pointer
			// If we already have the lock reply immediately
			DIST2_DEBUG("we already have the lock!\n");
			srs->error = DIST2_ERR_LOCK_ALREADY_OWNED;
			srs->rpc_reply(b, srs);
		} else {
			// Add to waiting list
			DIST2_DEBUG("lock already exists, add to waiting list!\n");

			char* lock_name = ast->on.name->in.str;
			size_t length = snprintf(NULL, 0, "%s_%lu", lock_name, enumerator++);
			char* wait_name = malloc(length+1);
			snprintf(wait_name, length+1, "%s_%lu", lock_name, enumerator);

			ast->on.name->in.str = wait_name;
			// TODO strdup
			ast_append_attribute(ast, ast_pair(ast_ident(strdup("wait_for")), ast_string(lock_name)));
			ast_append_attribute(ast, ast_pair(ast_ident(strdup("owner")), ast_num( (int64_t)b) )); // TODO strdup

			// free(lock_name); we don't need to do this because we put lock name back in the AST
			// so it's freed on calling free_ast().

			set_record(ast, &srs->query_state);
		}

		free_ast(ast2);
	}

	free_ast(ast);
	free(query);

	debug_printf("lock handler done\n");
}


static void unlock_reply(struct dist_binding* b, struct dist_reply_state* srs)
{
    errval_t err;
    err = b->tx_vtbl.unlock_response(b, MKCONT(free_dist_reply_state, srs),
			                          srs->error);
    if (err_is_fail(err)) {
        if(err_no(err) == FLOUNDER_ERR_TX_BUSY) {
        	dist_rpc_enqueue_reply(b, srs);
        	return;
        }
        USER_PANIC_ERR(err, "SKB sending %s failed!", __FUNCTION__);
    }
}


void unlock_handler(struct dist_binding* b, char* query)
{
	assert(query != NULL); // TODO
	errval_t err = SYS_ERR_OK;
	DIST2_DEBUG("unlock handler: %s\n", query);

	struct dist_reply_state* srs = NULL;
	err = new_dist_reply_state(&srs, unlock_reply);
	assert(err_is_ok(err));

	printf("before generate ast for query = %s\n", query);
	struct ast_object* query_ast = NULL;
	err = generate_ast(query, &query_ast);
	DEBUG_ERR(err, "generate ast for query = %s\n", query);
	assert(query_ast != NULL);

	if(err_is_ok(err)) {

		err = get_record(query_ast, &srs->query_state);
		if(err_is_ok(err)) {
			DIST2_DEBUG("found lock\n");

			struct ast_object* lock_ast = NULL;
			err = generate_ast(srs->query_state.stdout.buffer, &lock_ast);// TODO free
			assert(err_is_ok(err));
			assert(lock_ast != NULL);

			struct ast_object* owner = ast_find_attribute(lock_ast, "owner");
			assert(owner != NULL);

			if( ((void*)owner->pn.right->cn.value) != b) { // TODO compare int64_t vs. pointer
				// Cannot unlock if we're not the owner
				srs->error = DIST2_ERR_LOCK_NOT_OWNED;
				srs->rpc_reply(b, srs);
			}
			free_ast(lock_ast);

			char* findChild = "_ { wait_for: '%s' }";
			size_t length = snprintf(NULL, 0, findChild, query_ast->on.name->in.str);
			char buf[length+1]; // TODO stack or heap?
			snprintf(buf, length+1, findChild, query_ast->on.name->in.str);

			struct ast_object* next_child_ast = NULL;
			err = generate_ast(buf, &next_child_ast);
			assert(next_child_ast != NULL);
			assert(err_is_ok(err));

			struct dist_reply_state* child_srs = NULL;
			err = new_dist_reply_state(&child_srs, unlock_reply);
			assert(err_is_ok(err));

			DIST2_DEBUG("search wait_for element\n");

			err = get_record(next_child_ast, &child_srs->query_state);

			free_ast(next_child_ast);
			next_child_ast = NULL;

			if(err_is_ok(err)) {
				err = generate_ast(child_srs->query_state.stdout.buffer, &next_child_ast);
				assert(err_is_ok(err));
				assert(next_child_ast != NULL);

				// delete wait_for element
				del_record(next_child_ast, &child_srs->query_state);

				DIST2_DEBUG("remove wait_for\n");

				struct ast_object* wait_for = ast_remove_attribute(next_child_ast, "wait_for");
				assert(wait_for != NULL);

				free(next_child_ast->on.name->in.str);
				next_child_ast->on.name->in.str = wait_for->pn.right->in.str;
				wait_for->pn.right->in.str = NULL;

				free_ast(wait_for);

				// Set new lock record
				DIST2_DEBUG("set new lock obj\n");
				set_record(next_child_ast, &child_srs->query_state);

				// return lock() call of new owner
				owner = ast_find_attribute(next_child_ast, "owner");
				assert(owner != NULL);
				struct dist_binding* new_owner = (struct dist_binding*) owner->pn.right->cn.value;

				srs->error = SYS_ERR_OK;
				child_srs->rpc_reply(new_owner, child_srs);

			} else if(err_no(err) == DIST2_ERR_NO_RECORD) {
				// No one waits for lock, we're done.
				srs->error = SYS_ERR_OK;
			} else {
				// Return to client?
				assert(!"unlock_handler unexpected error code");
			}

			free_ast(next_child_ast);

		} else if(err_no(err) == DIST2_ERR_NO_RECORD) {
			srs->error = err_push(err, DIST2_ERR_NOT_LOCKED);
		} else {
			// Return to client?
			assert(!"unlock_handler unexpected error code");
		}

	}

	free_ast(query_ast);
	free(query);

	srs->rpc_reply(b, srs);
}
*/



static uint64_t current_id = 1;
void get_identifier(struct dist_binding* b)
{
	errval_t err  = b->tx_vtbl.get_identifier_response(b, NOP_CONT, current_id++);
	assert(err_is_ok(err));
}


void identify_events_binding(struct dist_event_binding* b, uint64_t id)
{
    errval_t err = set_binding(dist_BINDING_EVENT, id, b);
	assert(err_is_ok(err)); // TODO
}


void identify_rpc_binding(struct dist_binding* b, uint64_t id)
{
    errval_t err = set_binding(dist_BINDING_RPC, id, b);
    assert(err_is_ok(err));
	// Returning is done by prolog predicate C function!
}


