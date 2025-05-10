class Solution:
    def twoSum(self, numbers: List[int], target: int) -> List[int]:
        hashmap = {}
        for i in range(len(numbers)):
            ans = target - numbers[i]
            if ans in hashmap:
                return [hashmap[ans],i+1]
            hashmap[numbers[i]] = i+1
        return []
