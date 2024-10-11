/* SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause */
/* Copyright 2018 Hewlett Packard Enterprise Development LP */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

#include "libcxi_test_common.h"
#include "uapi/misc/cxi.h"

#define WIN_LENGTH (0x1000)
#define PUT_BUFFER_ID 0xb0f
#define GET_BUFFER_ID 0xa0e
#define PUT_DISABLED (false)
#define GET_DISABLED (false)

TestSuite(rma, .init = data_xfer_setup, .fini = data_xfer_teardown);

/* Test basic RMA Put command */
void rma_simple_put(void)
{
	struct mem_window src_mem;
	struct mem_window dst_mem;
	int pid_idx = 0;

	/* Allocate buffers */
	alloc_iobuf(WIN_LENGTH, &dst_mem, CXI_MAP_WRITE);
	alloc_iobuf(WIN_LENGTH, &src_mem, CXI_MAP_READ);

	/* Initialize Send Memory */
	for (int i = 0; i < src_mem.length; i++)
		src_mem.buffer[i] = i;

	/* Initialize RMA PtlTE and Post RMA Buffer */
	ptlte_setup(pid_idx, false);

	append_le_sync(rx_pte, &dst_mem, C_PTL_LIST_PRIORITY, PUT_BUFFER_ID,
		       0, 0, CXI_MATCH_ID_ANY, 0, true, false, false, false,
		       false, true, false, NULL);

	/* Do a set of xfers in increments until the whole source mem is xfered
	 * to the destination mem
	 */
	for (int xfer_len = 1; xfer_len <= src_mem.length; xfer_len <<= 1) {
		/* Initiate Put Operation */
		memset(dst_mem.buffer, 0, dst_mem.length);
		do_put_sync(src_mem, xfer_len, 0, 0, pid_idx, true, 0, 0, 0);

		/* Validate Source and Destination Data Match */
		for (int i = 0; i < xfer_len; i++)
			cr_expect_eq(src_mem.buffer[i],
				     dst_mem.buffer[i],
				     "Data mismatch: idx %4d - %02x != %02x",
				     i, src_mem.buffer[i], dst_mem.buffer[i]);
	}

	/* Clean up PTE and RMA buffer */
	unlink_le_sync(rx_pte, C_PTL_LIST_PRIORITY, PUT_BUFFER_ID);
	ptlte_teardown();

	/* Deallocate buffers */
	free_iobuf(&src_mem);
	free_iobuf(&dst_mem);
}

Test(rma, simple_put, .timeout = 15, .disabled = PUT_DISABLED)
{
	rma_simple_put();
}

/* Test basic RMA Get command */
Test(rma, simple_get, .timeout = 15, .disabled = GET_DISABLED)
{
	struct mem_window src_mem;
	struct mem_window dst_mem;
	int pid_idx = 0;

	/* Allocate buffers */
	alloc_iobuf(WIN_LENGTH, &src_mem, CXI_MAP_READ);
	alloc_iobuf(WIN_LENGTH, &dst_mem, CXI_MAP_WRITE);

	/* Initialize Source Memory */
	for (int i = 0; i < src_mem.length; i++)
		src_mem.buffer[i] = i;

	/* Set up PTE and RMA buffer */
	ptlte_setup(pid_idx, false);

	append_le_sync(rx_pte, &src_mem, C_PTL_LIST_PRIORITY, GET_BUFFER_ID,
		       0, 0, CXI_MATCH_ID_ANY, 0, true, false, false, false,
		       false, false, true, NULL);

	/* Do a set of xfers in increments until the whole source mem is xfered
	 * to the destination mem
	 */
	for (int xfer_len = 1; xfer_len <= src_mem.length; xfer_len <<= 1) {
		/* Initiate Get Operation */
		memset(dst_mem.buffer, 0, dst_mem.length);
		do_get_sync(dst_mem, xfer_len, 0, pid_idx, true, 0, 0, 0,
			     transmit_evtq);

		/* Validate Source and Destination Data Match */
		for (int i = 0; i < xfer_len; i++)
			cr_expect_eq(src_mem.buffer[i],
				     dst_mem.buffer[i],
				     "Data mismatch: idx %4d - %02x != %02x",
				     i, src_mem.buffer[i], dst_mem.buffer[i]);
	}

	/* Clean up PTE and RMA buffer */
	unlink_le_sync(rx_pte, C_PTL_LIST_PRIORITY, GET_BUFFER_ID);
	ptlte_teardown();

	/* Deallocate buffers */
	free_iobuf(&dst_mem);
	free_iobuf(&src_mem);
}

void data_xfer_setup_eqpt(void)
{
	transmit_eq_attr.flags = target_eq_attr.flags = CXI_EQ_PASSTHROUGH;
	data_xfer_setup();
}

TestSuite(rma_eqpt, .init = data_xfer_setup_eqpt, .fini = data_xfer_teardown);

Test(rma_eqpt, simple_put, .timeout = 5, .disabled = PUT_DISABLED)
{
	rma_simple_put();
}
