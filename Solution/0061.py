class Solution:
    def rotateRight(self, head: Optional[ListNode], k: int) -> Optional[ListNode]:
        if not head:
            return head

        #connect tail to head
        cur= head
        length =1
        while cur.next:
            cur = cur.next
            length+=1 
        cur.next = head

        #move to new head
        k= length - (k%length)
        for _ in range(k):
            cur=cur.next
            

        #disconnect and return new head
        newhead = cur.next
        cur.next=None
        return newhead
