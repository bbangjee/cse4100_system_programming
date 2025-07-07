# CSE4100: System Programming

## 🚀 프로젝트 목록

### Project 1: 자료구조 라이브러리 구현
- **디렉토리**: `prj01/`
- **주요 구현**: Bitmap, Hash Table, Linked List
- **파일**: `bitmap.c`, `hash.c`, `list.c`, `main.c`
- **특징**: 핀토스(PintOS) 운영체제에서 사용되는 자료구조들을 직접 구현

### Project 2: 나만의 셸 (MyShell)
- **디렉토리**: `prj02/`
- **단계별 구현**:
  - **Phase 1**: 기본 명령어 실행
  - **Phase 2**: 파이프라인 구현
  - **Phase 3**: 백그라운드/포그라운드 프로세스 구현
- **주요 기능**: 
  - 명령어 파싱 및 실행
  - 백그라운드/포그라운드 프로세스 관리
  - 파이프(`|`) 지원

### Project 3: 동시성 주식 서버 (Concurrent Stock Server)
- **디렉토리**: `prj03/`
- **구현 방식**:
  - **Task 1**: Event-driven 방식 (select 사용)
  - **Task 2**: Thread-based 방식 (pthread 사용)
- **주요 기능**:
  - 다중 클라이언트 동시 처리
  - 주식 거래 시뮬레이션 (buy/sell/show)
  - 성능 비교 분석

### Project 4: 동적 메모리 할당자 (Mallocator)
- **디렉토리**: `prj04/`
- **구현 함수**: `mm_malloc`, `mm_free`, `mm_realloc` 등
- **주요 특징**:
  - Explicit free list 사용
  - Best-fit 할당 정책
  - Boundary tag coalescing

---


## 🛠 실행 방법

### 환경 요구사항
- **OS**: Linux (Ubuntu)
- **Compiler**: GCC
- **Tools**: Make, GDB

### 주요 명령어 예시

```bash
# 프로젝트 1: 자료구조 테스트
./main < input.txt

# 프로젝트 2: 셸 실행
./myshell

# 프로젝트 3: 주식 서버
./stockserver portnum         # 서버 실행 (할당받은 포트 번호)
./stockclient IPaddr portnum  # 클라이언트 연결 (ip 주소, 할당 포트 번호)

# 프로젝트 4: 메모리 할당자
./mdriver -f short1-bal.rep -v  # 특정 트레이스 파일로 테스트
```

---

## 🏆 결과
### 📊 프로젝트별 점수

| 프로젝트 | 세부 점수 | 총점 |
|---------|-----------|------|
| **prj01** | Functionality: 95/95 + Shuffle: 2.5/2.5 + Hash: 2.5/2.5 + 보고서: 100/100 | **100/100**  |
| **prj02** | Phase1: 30/30 + Phase2: 24/30 + Phase3: 40/40 + 보고서: 20/20 | **114/120**  |
| **prj03** | Task1: 30/30 + Task2: 30/30 + Task3: 30/30 + 보고서: 10/10 | **100/100**  |
| **prj04** |  | **92/100**  |


---


