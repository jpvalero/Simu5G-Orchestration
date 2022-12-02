#!/usr/bin/env python
# coding: utf-8 

# k: current number of tasks
# m: number of servers
# n: server capacity

def orchestration(k, m, n, method): 
    if method == 'by_threshold':
        thr = 2                      # to be defined manually  
        if k >= m*n - thr:
            return 1                 # activate new server
        elif k < (m-1)*n - 1: 
            return -1                # deactivate server  
        else: 
            return 0                 # no action required 
        
        
            
if __name__ == "__main__":
    tasks = [3, 3, 6, 7, 2, 1, 7, 8, 9, 0, 5, 3, 2]        
    m = 3
    n = 3 
    method = 'by_threshold'
    for k in tasks: 
        action = orchestration(k, m, n, method)
        print('action: ', action) 