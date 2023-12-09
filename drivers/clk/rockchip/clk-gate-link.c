// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Fuzhou Rockchip Electronics Co., Ltd
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include "clk.h"

/**
 * struct clk_gate_link - gating link clock
 *
 * @gate: handle clk gate
 * @link: links clk
 */
struct clk_gate_link {
	struct clk_gate gate;
	struct clk	*link;
};

#define to_clk_gate_link(_gate) container_of(_gate, struct clk_gate_link, gate)

static int clk_gate_link_enable(struct clk_hw *hw)
{
	struct clk_gate_link *gate = to_clk_gate_link(to_clk_gate(hw));

	clk_gate_endisable(hw, 1);
	clk_enable(gate->link);

	return 0;
}

static void clk_gate_link_disable(struct clk_hw *hw)
{
	struct clk_gate_link *gate = to_clk_gate_link(to_clk_gate(hw));

	clk_gate_endisable(hw, 0);
	clk_disable(gate->link);
}

static int clk_gate_link_is_enabled(struct clk_hw *hw)
{
	return clk_gate_is_enabled(hw);
}

int clk_gate_link_prepare(struct clk_hw *hw)
{
	struct clk_gate_link *gate = to_clk_gate_link(to_clk_gate(hw));

	return clk_prepare(gate->link);
}

void clk_gate_link_unprepare(struct clk_hw *hw)
{
	struct clk_gate_link *gate = to_clk_gate_link(to_clk_gate(hw));

	clk_unprepare(gate->link);
}

const struct clk_ops clk_gate_link_ops = {
	.prepare = clk_gate_link_prepare,
	.unprepare = clk_gate_link_unprepare,
	.enable = clk_gate_link_enable,
	.disable = clk_gate_link_disable,
	.is_enabled = clk_gate_link_is_enabled,
};

struct clk *rockchip_clk_register_gate_link(struct rockchip_clk_provider *ctx,
					    const char *name, const char *parent_name,
					    unsigned int link_id, u8 flags,
					    void __iomem *gate_offset, u8 gate_shift,
					    u8 gate_flags, spinlock_t *lock)
{
	struct clk_gate_link *gate_link;
	struct clk_init_data init = {};
	struct clk **clks;
	struct clk *clk_link;

	if (gate_flags & CLK_GATE_HIWORD_MASK) {
		if (gate_shift > 15) {
			pr_err("gate bit exceeds LOWORD field\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	/* allocate the gate */
	gate_link = kzalloc(sizeof(*gate_link), GFP_KERNEL);
	if (!gate_link)
		return ERR_PTR(-ENOMEM);

	clks = ctx->clk_data.clks;
	gate_link->link = clks[link_id];

	init.name = name;
	init.ops = &clk_gate_link_ops;
	init.flags = flags;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = 1;

	/* struct clk_gate assignments */
	gate_link->gate.reg = gate_offset;
	gate_link->gate.bit_idx = gate_shift;
	gate_link->gate.flags = gate_flags;
	gate_link->gate.lock = lock;
	gate_link->gate.hw.init = &init;

	clk_link = clk_register(NULL, &gate_link->gate.hw);
	if (IS_ERR(clk_link)) {
		kfree(gate_link);
		pr_err("%s clk_register field\n", name);
		return ERR_CAST(clk_link);
	}

	return clk_link;
}
EXPORT_SYMBOL_GPL(rockchip_clk_register_gate_link);
