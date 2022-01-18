#!/usr/bin/env python

import sys
import pandas as pd
import matplotlib.pyplot as plt

df  = pd.read_csv(sys.argv[1])
df.plot(kind='scatter',x='start',y='latency',s=1) # scatter plot
# df.plot(kind='hist')  # histogram

plt.savefig('foo-scatter.pdf')
