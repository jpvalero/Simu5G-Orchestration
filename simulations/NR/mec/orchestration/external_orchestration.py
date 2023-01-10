#!/usr/bin/env python
# coding: utf-8 

import sys

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
        
        
            
if __name__ == "__main__":
    
    # print(f"Arguments count: {len(sys.argv)}")
    # for i, arg in enumerate(sys.argv):
    #   print(f"Argument {i:>6}: {arg}")
        
    tasks = int(sys.argv[1])        
    m = int(sys.argv[2])
    n = int(sys.argv[3]) 
    method = 'by_threshold'
    
    action = orchestration(tasks, m, n, method)
    print(action) 
    