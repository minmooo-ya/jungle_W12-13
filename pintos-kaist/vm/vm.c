/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

struct frame_table *frame_table;

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* 이 위쪽은 수정하지 마세요 !! */
	/* TODO: 이 아래쪽부터 코드를 추가하세요 */

	frame_table_init();
}

/* 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 후 타입을 알고 싶을 때 유용합니다.
 * 이 함수는 이미 완전히 구현되어 있습니다. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

void frame_table_init(){
	frame_table = malloc(sizeof(struct frame_table));
	list_init(&frame_table->frame_list);
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* 초기화 함수와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 직접 생성하지 말고,
 * 반드시 이 함수나 `vm_alloc_page`를 통해 생성하세요. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* 이미 해당 page가 SPT에 존재하는지 확인합니다 */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: VM 타입에 따라 페이지를 생성하고, 초기화 함수를 가져온 뒤,
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
		 * TODO: uninit_new 호출 후에는 필요한 필드를 수정해야 합니다. */
		bool (*page_initializer)(struct page *, enum vm_type, void *kva);
		struct page *page = malloc(sizeof(struct page));
		page->writable = writable;

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_MMAP:
			/* 매핑 카운트를 추가해두자
			   mmap_list로 mmap 페이지를 관리할거면 필요 x */
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		default:
			free(page);
			goto err;
			break;
		}

		uninit_new(page, upage, init, type, aux, page_initializer);

		/* TODO: 생성한 페이지를 spt에 삽입하세요. */
		hash_insert(&spt->spt_hash, &page->hash_elem);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 가상 주소를 통해 SPT에서 페이지를 찾아 리턴합니다.
 * 에러가 발생하면 NULL을 리턴하세요 */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page page;
	/* TODO: Fill this function. */
	page.va=pg_round_down(va);
	
	struct hash_elem *e = hash_find(&spt->spt_hash, &page.hash_elem);
	if(&e==NULL)
		return NULL;
	else	
		return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */
	
	struct hash_elem * e=hash_insert(&spt->spt_hash, &page->hash_elem);
	if(e!=NULL) return succ; //실패했음

	succ=true;
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	/** TODO: page 해제
	 * 매핑된 프레임을 해제해야하나?
	 * 프레임이 스왑되어있는지 체크할것?
	 * 아마 pml4_clear_page 사용하면 된대요
	 */
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim;
	/* TODO: 교체 정책을 여기서 구현해서 희생자 페이지 찾기 */

	victim= list_pop_front(&frame_table->frame_list);

	return victim;
}

/* 한 페이지를 교체(evict)하고 해당 프레임을 반환합니다.
 * 에러가 발생하면 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();


	/** TODO: 여기서 swap_out 매크로를 호출??
	 *	pml4_clear_page를 아마 사용?? (잘 모름)
	 */

	return NULL;
}

/* palloc()을 사용하여 프레임을 할당합니다.
 * 사용 가능한 페이지가 없으면 페이지를 교체(evict)하여 반환합니다.
 * 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면,
 * 이 함수는 프레임을 교체하여 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = palloc_get_page(PAL_USER);
	if(frame ==NULL){
		frame=vm_evict_frame(); //이 안에서 swap out
	}

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	/* 스택 최하단에 익명 페이지를 추가하여 사용
	 * addr은 PGSIZE로 내림(정렬)하여 사용	 */
	vm_alloc_page(VM_ANON, addr, true); // 스택 최하단에 익명 페이지 추가
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
/* 인터럽트 프레임, addr=폴트를 일으킨 주소(코드일 수도있고 데이터일수도 있음),
user=사용자 접근인지 커널 접근인지, write=true면 쓰기 허용 false면 읽기만
not_present: true면 존재하지 않는 페이지, false면  */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{


	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page;	

	spt_find_page(spt, addr);

	/* TODO: Validate the fault */
	/* bogus 폴트인지? 스택확장 폴트인지?
	 * SPT 뒤져서 존재하면 bogus 폴트!!
	 * addr이 유저 스택 시작 주소 + 1MB를 넘지 않으면 스택확장 폴트
	 * 찐폴트면 false 리턴
	 * 아니면 vm_do_claim_page 호출	*/

	/* 스택확장 폴트에서 valid를 확인하려면 유저 스택 시작 주소 + 1MB를 넘는지 확인
	 * addr = thread 내의 user_rsp
	 * addr은 user_rsp보다 크면 안됨
	 * stack_growth 호출해야함 */

	/* TODO: Your code goes here */

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* VA에 할당된 페이지를 요구합니다 . */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = spt_find_page(&thread_current()->spt, va);
	/* TODO: Fill this function */
	if(page==NULL) return false;

	return vm_do_claim_page(page);
}

/* PAGE를 요구하고 mmu를 설정합니다*/
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

bool is_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	const struct page *page_a=hash_entry(a, struct page, hash_elem);
	const struct page *page_b=hash_entry(b, struct page, hash_elem);
	return page_a->va < page_b->va;
}

size_t page_hash(const struct hash_elem *e, void * aux){
	const struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->spt_hash, page_hash, is_less, NULL);
}



/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 스토리지에 기록(writeback)하세요. */
	
	//spt_remove_page 호출해야 할 듯.
}