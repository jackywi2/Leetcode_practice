class Solution:
    def containsDuplicate(self, nums: List[int]) -> bool:
        l=nums
        sett=set(nums)
        if len(l)==len(sett):
            return False
        else:
            return True
