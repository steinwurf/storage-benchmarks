#!/usr/bin/env python
"""
Copyright Steinwurf ApS 2011-2013.
Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
See accompanying file LICENSE.rst or
http://www.steinwurf.com/licensing

Plot the throughput for all benchmarked Kodo platforms
"""

import pandas as pd
import scipy as sp
import pylab as pl

import sys
sys.path.insert(0, "../")
import plot_helper as ph

def plot(args):
    plotter = ph.plotter(args)

    query = {
    "type": args.coder,
    "branch" : "master",
    "scheduler": "kodo-nightly-benchmark",
    "utc_date" : {"$gte": args.date - ph.timedelta(1), "$lt": args.date},
    }
    df = plotter.get_dataframe(query)

    df['mean'] = df['throughput'].apply(sp.mean)
    df['std'] = df['throughput'].apply(sp.std)
    df['erasures'] = df['erasure_rate']

    # Group by type of code; dense, sparse
    dense = df[df['testcase'] == "FullDelayedRLNC"].groupby(by=
        ['slavename'])
    sparse = df[df['testcase'] == "SparseDelayedRLNC"].groupby(by=
        ['slavename'])

    def plot_setup(p):
        pl.ylabel("Throughput" + " [" + list(group['unit'])[0] + "]")
        pl.xticks(list(sp.unique(group['erasures'])))
        plotter.set_markers(p)
        plotter.set_slave_info(slavename)

    for (slavename), group in dense:
        p = group.pivot_table('mean',  rows='erasures', cols=['symbols',
        'symbol_size']).plot()
        plot_setup(p)
        plotter.write("dense", slavename)

    for (slavename), group in sparse:
        p = group.pivot_table('mean',  rows='erasures', cols=['symbols',
        'symbol_size']).plot()
        plot_setup(p)
        plotter.write("sparse", slavename)

    return df

if __name__ == '__main__':


    args = ph.add_arguments(["json", "coder", "output-format", "date"])
    df = plot(args)
