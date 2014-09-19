# -*- coding: utf-8 -*-
"""
Created on Mon Aug 25 19:30:05 2014

@author: nestor
"""

import glob
import pandas as pd
import plot_storage_helper as ph

#Get the data from different files for the different algorithms and libraries
df_openfec = pd.concat(
    [pd.read_csv(f) for f in glob.glob('openfec_debian6*.csv')])
df_jerasure = pd.concat(
    [pd.read_csv(f) for f in glob.glob('jerasure_debian6*.csv')])
df_isa = pd.concat(
    [pd.read_csv(f) for f in glob.glob('isa_debian6*.csv')])
df_fullrlnc = pd.concat(
    [pd.read_csv(f) for f in glob.glob('fullrlnc_debian6*.csv')])
df_sparse = pd.concat(
    [pd.read_csv(f) for f in glob.glob('sparse_debian6*.csv')])
df_perpetual = pd.concat(
    [pd.read_csv(f) for f in glob.glob('perpetual_debian6*.csv')])
df_thread = pd.concat(
    [pd.read_csv(f) for f in glob.glob('thread_rlnc_debian6*.csv')])
df_sparse_thread = pd.concat(
    [pd.read_csv(f) for f in glob.glob('sparse_thread_debian6*.csv')])

#Patch dataframes

#Density for non-sparse
df_openfec['density'] = 1
df_isa['density'] = 1
df_jerasure['density'] = 1
df_fullrlnc['density'] = 1
df_thread['density'] = 1

#Rename perpetual dataframe 'width_ratio' column as 'density' because the
#width ratio can be seen as a density
df_perpetual = df_perpetual.rename(columns = {'width_ratio':'density'})

#Concatenate all dataframes into a single one
df_all_sparse = pd.concat([df_openfec, df_perpetual,
                           df_sparse_thread[(
                           df_sparse_thread['density'] != 0.5)],
                           df_sparse[df_sparse['density'] != 0.5 ]])

df_all_dense = pd.concat([df_isa, df_jerasure, df_fullrlnc,df_thread,
                          df_sparse[df_sparse['density'] == 0.5 ],
                          df_sparse_thread[df_sparse_thread['density'] == 0.5 ]
                          ])
#Note: Sparse RLNC (threaded or not) with 50% density can be regarded as dense

#Goodput dataframe vs. symbols (Fixed: symbol size, loss rate, type)
ph.plot_metric(df_all_sparse,'goodput','symbols',
              ['symbol_size','loss_rate','type'],
              ['testcase','density'],'sparse')

ph.plot_metric(df_all_dense,'goodput','symbols',
               ['symbol_size','loss_rate','type'],
               ['testcase','density'],'dense')

#Goodput dataframe vs. symbol size (Fixed: symbols, loss rate, type)
ph.plot_metric(df_all_sparse,'goodput','symbol_size',
               ['symbols','loss_rate','type'],
               ['testcase','density'],'sparse')

ph.plot_metric(df_all_dense,'goodput','symbol_size',
               ['symbols','loss_rate','type'],
               ['testcase','density'],'dense')

#Goodput dataframe vs. loss rate (Fixed: symbols, loss rate, type)
ph.plot_metric(df_all_sparse,'goodput','loss_rate',
               ['symbol_size','symbols','type'],
               ['testcase','density'],'sparse')

ph.plot_metric(df_all_dense,'goodput','loss_rate',
               ['symbol_size','symbols','type'],
               ['testcase','density'],'dense')

#Goodput dataframe vs. erased symbols (Fixed: symbol size, loss rate, type)
ph.plot_metric(df_all_sparse,'goodput','erased_symbols',
               ['symbol_size','symbols','type'],
               ['testcase','density'],'sparse')

ph.plot_metric(df_all_dense,'goodput','erased_symbols',
               ['symbol_size','symbols','type'],
               ['testcase','density'],'dense')

#Dataframe for checking linear dependency (overhead)
df_linear_dependency = df_all_sparse[df_all_sparse['type'] == "decoder"]

#Goodput dataframe vs. erased symbols (Fixed: symbol size, loss rate, type)
ph.plot_metric(df_linear_dependency,'extra_symbols','symbols',
               ['symbol_size','loss_rate'],
               ['testcase','density'],'sparse')