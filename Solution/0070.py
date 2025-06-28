class Solution:
    def climbStairs(self, n: int) -> int:
        if n == 1:
            return 1
        elif n == 2:
            return 2

        f = [None] * n
        f[0] = 1
        f[1] = 2
        for i in range(2,n):
            f[i] = f[i-2] + f[i-1]
        
        return f[-1]
