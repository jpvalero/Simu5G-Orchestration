#!/usr/bin/env python
# coding: utf-8 

import sys
from numpy import double

# k: current number of tasks
# m: number of servers
# n: server capacity

def orchestration(k, m, n, method): 
    if method == 'by_threshold':
        thr = 5                      # to be defined manually  
        if k >= m*n - thr:
            return 1                 # activate new server
        elif k < (m-1)*n - 1: 
            return -1                # deactivate server  
        else: 
            return 0                 # no action required 
    elif method == 'reactive':                  
        if k == m*n:                 # activation when reaching maximum capacity 
            return 1                  
        elif k < (m-1)*n - 1: 
            return -1                  
        else: 
            return 0    
    elif method == 'conservative':   # activation when reaching percentage
        percentage_activation = 0.5
        percentage_deactivation = 0.2 
        if k >= percentage_activation*m*n:  
            return 1 
        elif k <= percentage_deactivation*m*n: 
            return -1
        else: 
            return 0 
        
        
            
if __name__ == "__main__":
    
    # print(f"Arguments count: {len(sys.argv)}")
    # for i, arg in enumerate(sys.argv):
    #   print(f"Argument {i:>6}: {arg}")
        
    tasks = int(sys.argv[1])        
    m = int(sys.argv[2])
    n = int(sys.argv[3]) 
    time = double(sys.argv[4])
    method = 'by_threshold'
    
    action = orchestration(tasks, m, n, method)
    print(action) 
    