/*
   Unix SMB/CIFS implementation.
   async queryuser
   Copyright (C) Volker Lendecke 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "winbindd.h"
#include "librpc/gen_ndr/ndr_wbint_c.h"
#include "../libcli/security/security.h"

struct wb_queryuser_state {
	struct dom_sid sid;
	struct wbint_userinfo *info;
};

static void wb_queryuser_done(struct tevent_req *subreq);

struct tevent_req *wb_queryuser_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     const struct dom_sid *user_sid)
{
	struct tevent_req *req, *subreq;
	struct wb_queryuser_state *state;
	struct winbindd_domain *domain;

	req = tevent_req_create(mem_ctx, &state, struct wb_queryuser_state);
	if (req == NULL) {
		return NULL;
	}
	sid_copy(&state->sid, user_sid);

	domain = find_domain_from_sid_noinit(user_sid);
	if (domain == NULL) {
		tevent_req_nterror(req, NT_STATUS_NO_SUCH_USER);
		return tevent_req_post(req, ev);
	}

	state->info = talloc(state, struct wbint_userinfo);
	if (tevent_req_nomem(state->info, req)) {
		return tevent_req_post(req, ev);
	}

	subreq = dcerpc_wbint_QueryUser_send(state, ev, dom_child_handle(domain),
					     &state->sid, state->info);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, wb_queryuser_done, req);
	return req;
}

static void wb_queryuser_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct wb_queryuser_state *state = tevent_req_data(
		req, struct wb_queryuser_state);
	NTSTATUS status, result;

	status = dcerpc_wbint_QueryUser_recv(subreq, state->info, &result);
	TALLOC_FREE(subreq);
	if (any_nt_status_not_ok(status, result, &status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS wb_queryuser_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			   struct wbint_userinfo **pinfo)
{
	struct wb_queryuser_state *state = tevent_req_data(
		req, struct wb_queryuser_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pinfo = talloc_move(mem_ctx, &state->info);
	return NT_STATUS_OK;
}
