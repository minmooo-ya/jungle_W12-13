/* uninit.c: 미초기화(uninitialized) 페이지의 구현.
 *
 * 모든 페이지는 처음에 uninit 페이지로 생성됩니다. 첫 번째 페이지 폴트가 발생하면,
 * 핸들러 체인이 uninit_initialize(page->operations.swap_in)를 호출합니다.
 * uninit_initialize 함수는 페이지 객체를 초기화하여 해당 페이지를
 * 특정 페이지 객체(anon, file, page_cache)로 변환(transmute)하고,
 * vm_alloc_page_with_initializer 함수에서 전달된 초기화 콜백을 호출합니다.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* 첫 번째 폴트 시 페이지를 초기화합니다 */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* 먼저 가져옵니다. page_initialize가 값을 덮어쓸 수 있습니다. */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: 이 함수를 수정해야 할 수도 있습니다. */
	//여기에서 init이 존재하면 lazy_load_segment(page, aux)가 호출됨. 없는 경우 그냥 true -> 그래서 수정이 필요한 걸수도 
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* uninit_page가 보유한 리소스를 해제합니다. 대부분의 페이지는 다른 페이지 객체로 변환되지만,
 * 실행 중 한 번도 참조되지 않은 uninit 페이지가 프로세스 종료 시 남아 있을 수 있습니다.
 * PAGE 자체는 호출자가 해제합니다. */
static void
uninit_destroy (struct page *page) {
	struct file_info *aux = (struct file_info *)page->uninit.aux;
    if (aux != NULL) {
        file_close(aux->file);  // 파일 핸들 닫기
        free(aux);              // aux 메모리 해제
    }
	return;
}