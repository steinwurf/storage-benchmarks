# -*- coding: utf-8 -*-
"""
Created on Fri Sep 12 20:41:09 2014

@author: nestor
"""

import matplotlib.pyplot as plt
import scipy as sp
from matplotlib.backends.backend_pdf import PdfPages as pp

xlabel_names = { 'symbol_size' : r"$Symbol\ Size\ [KB]$",
                 'symbols' : r"$Symbols$",
                 'loss_rate' : r"$Loss\ Rate\ [\%]$",
                 'erased_symbols' : r"$Erased\ Symbols$"
}

ylabel_names = { 'goodput' : r"$Goodput\ [MB/s]$",
                 'extra_symbols' : r"$Average\ Extra\ Symbols$"
}

title_names = { 'symbol_size' : r"$\ Packet\ size\colon\ $",
                'symbols' : r"$\ Symbols\colon\ $",
                'loss_rate' : r"$\ Loss\ rate\colon\ $",
                'type' : r"$\ Device\colon\ $"
}

label_names = { u'(OpenFEC, 1.0)' : "$OpenFEC-LDPC$",
                u'(Perpetual, 0.2652)' : "$Kodo-Perpetual,\ w_r =\ 0.2652$",
                u'(Perpetual, 0.375)' : "$Kodo-Perpetual,\ w_r =\ 0.375$",
                u'(Perpetual, 0.5303)' : "$Kodo-Perpetual,\ w_r =\ 0.5303$",
                u'(SparseFullRLNC, 0.3)' : "$Kodo-Sparse\ RLNC,\ d =\ 0.3$",
                u'(SparseFullRLNC, 0.4)' : "$Kodo-Sparse\ RLNC,\ d =\ 0.4$",
                u'(SparseFullRLNC, 0.5)' : "$Kodo-Sparse\ RLNC,\ d =\ 0.5$",
                u'(SparseThread, 0.3)' : "$Kodo-Sparse\ Threading,\ d =\ 0.3$",
                u'(SparseThread, 0.4)' : "$Kodo-Sparse\ Threading,\ d =\ 0.4$",
                u'(SparseThread, 0.5)' : "$Kodo-Sparse\ Threading,\ d =\ 0.5$",
                u'(Thread, 1.0)' : "$Kodo-Threading\ RLNC$",
                u'(FullRLNC, 1.0)' : "$Kodo-Full\ RLNC$",
                u'(ISA, 1.0)' : "$ISA-RS$",
                u'(Jerasure, 1.0)' : "$Jerasure-RS$",
}

def title_symbol_size(symbol_size):
    return '$' + str(symbol_size/1000) + '\ KB.$'

def title_loss_rate(loss_rate):
    return '$' + str(int(loss_rate*100)) + '\%.$'

def title_device_type(device):
    return '$' + device.title() + '.$'

parameter_functions = {'symbol_size' : title_symbol_size,
                       'loss_rate' : title_loss_rate,
                       'type' : title_device_type,
}

def set_parameter(parameter,parameters_list,keys):
    if parameter not in parameter_functions.keys():
        return '$' + str(keys[parameters_list.index(parameter)]) + '.$'
    else:
        return parameter_functions[parameter](
        keys[parameters_list.index(parameter)])

def get_plot_title(parameters_list,keys):
    title = r''
    for parameter in parameters_list:
        title += title_names[parameter] + set_parameter(parameter,
                                                        parameters_list,
                                                        keys)
    return title

xscale_arguments = { ('goodput','symbol_size') : ['log',dict(basex=2)],
                     ('goodput','symbols') : ['log',dict(basex=2)],
                     ('goodput','loss_rate') : ['linear',dict()],
                     ('goodput','erased_symbols') : ['linear',dict()],
                     ('extra_symbols','symbols') : ['linear',dict()]
}

symbol_size_label = { 32000 : r"$32$",
                      64000 : r"$64$",
                      128000 : r"$128$",
                      256000 : r"$256$",
                      512000 : r"$512$",
                      1024000 : r"$1024$"
}

symbols_label = { 8 : r"$8$",
                  16 : r"$16$",
                  32 : r"$32$",
                  64 : r"$64$",
                  128 : r"$128$",
                  256 : r"$256$",
                  512 : r"$512$"
}

loss_rate_label = { 0.05 : r"$5$",
                    0.1 : r"$10$",
                    0.15 : r"$15$",
                    0.2 : r"$20$",
                    0.25 : r"$25$",
                    0.3 : r"$30$"
}

erased_symbols_label = {
}

varying_xlabels = {'symbol_size' : symbol_size_label,
                   'symbols' : symbols_label,
                   'loss_rate' : loss_rate_label,
                   'erased_symbols' : erased_symbols_label
}

pltkind = { ('goodput','symbol_size') : 'line',
           ('goodput','symbols') : 'line',
           ('goodput','loss_rate') : 'line',
           ('goodput','erased_symbols') : 'bar',
           ('extra_symbols','symbols') : 'bar'
}

def set_axis_properties(p,metric,varying_parameter,group):

    #Set major x-axis label
    plt.xlabel(xlabel_names[varying_parameter])

    #Set x-axis scale
    xscale_args = xscale_arguments[(metric,varying_parameter)]
    plt.xscale(xscale_args[0],**xscale_args[1])

    #Set x-axis tick labels
    #Get tick values
    ticks = list(sp.unique(group[varying_parameter]))

    #If an item is not in the tick dictionary for the bar plot, add it
    if pltkind[(metric,varying_parameter)] is 'bar':
        for item in ticks:
            if item not in varying_xlabels[varying_parameter].keys():
                varying_xlabels[varying_parameter][item] = '$' + str(item) +'$'

    xlabels = [ varying_xlabels[varying_parameter][item] for item in ticks]

    if pltkind[(metric,varying_parameter)] is 'bar':
        p.set_xticks(sp.arange(len(ticks))+0.5)
        plt.setp(p.set_xticklabels(xlabels), rotation=0)
    else:
        plt.xticks(ticks,xlabels)

    plt.ylabel(ylabel_names[metric])
    plt.grid('on')

def set_plot_legend(p,metric,varying_parameter):

    lines, labels = p.get_legend_handles_labels()

    if pltkind[(metric,varying_parameter)] is 'line':
        for l in lines:
            l.set_marker(markers[lines.index(l)])
    for lb in labels:
        labels[labels.index(lb)] = label_names[lb]

    p.legend(lines,labels,loc='center left',ncol=1,fontsize='small',
             bbox_to_anchor=(1, 0.5))

def get_filename(metric,varying_parameter,fixed_parameters,keys,density):

    fixed_values = ""
    for fixed in fixed_parameters:
        fixed_values += "_" + fixed + "_" + \
                        str(keys[fixed_parameters.index(fixed)])

    filename = density + "_" + metric + "_" + varying_parameter + \
               fixed_values + ".pdf"
    return filename


def plot_metric(df,metric,varying_parameter,fixed_parameters,cases,density):

    df_group = df.groupby(by=fixed_parameters)
    all_figures_filename = "all_" + density + "_" + metric + "_vs_" + \
                           varying_parameter + ".pdf"
    pdf = pp(all_figures_filename)
    for keys, group in df_group:

        p = group.pivot_table(metric,cols=cases,rows=varying_parameter).plot(
            kind=pltkind[(metric,varying_parameter)])

        plt.title(get_plot_title(fixed_parameters,keys),fontsize=font_size)
        set_axis_properties(p,metric,varying_parameter,group)
        set_plot_legend(p,metric,varying_parameter)
        filename = get_filename(metric,varying_parameter,
                                fixed_parameters,keys,density)
        plt.savefig(filename,bbox_inches='tight')
        pdf.savefig(bbox_inches='tight')
        plt.close()

    pdf.close()

######################## PLOTTING SETTINGS ################################
font_size=14

font = {'family' : 'sans-serif',
        'weight' : 'medium',
        'style'  : 'normal',
        'size'   : font_size}
plt.rc('font', **font)
plt.rc('text', usetex=True)
plt.rc('xtick',labelsize=font_size)
plt.rc('ytick',labelsize=font_size)

colors = ['SteelBlue','DarkBlue',
          'LimeGreen','DarkGreen',
          'Crimson','DarkRed',
          'Brown','Black']

plt.rc('axes', color_cycle = colors)

markers = ["d","+","x","^","v","s","*","h"]
