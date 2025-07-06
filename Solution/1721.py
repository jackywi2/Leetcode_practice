# Definition for singly-linked list.
# class ListNode:
#     def __init__(self, val=0, next=None):
#         self.val = val
#         self.next = next
class Solution:
    def swapNodes(self, head: Optional[ListNode], k: int) -> Optional[ListNode]:
        first = head
        for _ in range(k-1):
            first = first.next

        runner = first
        second = head
        while(runner.next):
            runner = runner.next
            second = second.next

        first.val, second.val = second.val, first.val

        return head
