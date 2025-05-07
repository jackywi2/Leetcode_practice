class Solution:
    def sortArray(self, nums: List[int]) -> List[int]:
        if len(nums) > 1:
            mid = len(nums) // 2
            right_nums = nums[:mid]
            left_nums = nums[mid:]
            self.sortArray(right_nums)
            self.sortArray(left_nums)
            r_index = 0
            l_index = 0
            m_index = 0
            while (r_index < len(right_nums)) and (l_index < len(left_nums)):
                if left_nums[l_index] < right_nums[r_index]:
                    nums[m_index] = left_nums[l_index]
                    m_index += 1
                    l_index += 1
                else:
                    nums[m_index] = right_nums[r_index]
                    m_index += 1
                    r_index += 1
            while l_index < len(left_nums):
                nums[m_index] = left_nums[l_index]
                m_index += 1
                l_index += 1
            while r_index < len(right_nums):
                nums[m_index] = right_nums[r_index]
                m_index += 1
                r_index += 1
        return nums
