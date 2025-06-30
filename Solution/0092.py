# Definition for singly-linked list.
# class ListNode:
#     def __init__(self, val=0, next=None):
#         self.val = val
#         self.next = next
class Solution:
    def reverseBetween(self, head: Optional[ListNode], left: int, right: int) -> Optional[ListNode]:
        output = ListNode(0, head)
        p = output

        for i in range(left-1):
            p = p.next

        q = p.next
        for i in range(right - left):
            r = q.next
            q.next = r.next
            r.next = p.next 
            p.next = r
        
        return output.next
