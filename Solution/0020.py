class Solution:
    def isValid(self, s: str) -> bool:
        pstack = []
        pMap = {")": "(","}": "{","]": "[" }
        for ch in s:
            if pstack:
                if ch in pMap:
                    if pstack[-1] != pMap[ch]:
                        return False
                    pstack.pop()
                    continue
            pstack.append(ch)
        
        return not pstack     
