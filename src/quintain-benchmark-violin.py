#!/usr/bin/env python

import sys
import numpy as np
import matplotlib.pyplot as plt
plt.rcParams.update({'font.size': 13})
# if using a Jupyter notebook, include:
# %matplotlib inline

latencies = np.loadtxt(sys.argv[1], unpack=True)

data = list([latencies])

fig, ax = plt.subplots()

# build a violin plot
ax.violinplot(data, showmeans=False, showmedians=True, showextrema=True)

# add title and axis labels
# ax.set_title('violin plot')
ax.set_ylabel('Latency (seconds)')
# ax.set_xlabel('Memory buffer strategy')

# add x-tick labels
# xticklabels = ['default', 'patched']
# ax.set_xticks([1,2])
# ax.set_xticklabels(xticklabels)

# add horizontal grid lines
ax.yaxis.grid(True)

# show the plot
plt.ylim(bottom=0)
plt.tight_layout()
# plt.show()
plt.savefig('foo.pdf')
