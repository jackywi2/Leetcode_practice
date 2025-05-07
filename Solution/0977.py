class Solution:
    def sortedSquares(self, nums: List[int]) -> List[int]:
        output = [num*num for num in nums]
        output.sort()
        return output
