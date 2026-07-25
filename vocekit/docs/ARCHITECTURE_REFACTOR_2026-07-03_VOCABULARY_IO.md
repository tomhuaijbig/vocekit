# 架构拆分记录：词库导入导出规则下沉

更新时间：2026-07-03

## 本轮完成

- 新增 `src/domain/vocabulary_io.h/.cpp`。
- 将词库页里的导入解析、导出文本生成、作用范围归一化、匹配方式归一化和搜索筛选规则集中到独立模块。
- `src/pages/hub_vocabulary_page_methods.h` 保留 UI 调用入口，但对应方法已经变成薄包装，具体规则不再写在页面方法里。
- 新增 `tests/domain/vocabulary_io_tests.cpp` 和 `tests/domain/vocabulary_io_tests.pro`。

## 迁出的规则

- 内置作用范围：全部、全局、听写、翻译、问答。
- 自定义功能作用范围：通过 `VocabularyScopeOption` 传入，导入时可以用 id 或显示名称匹配。
- CSV 导入：支持英文表头和中文表头。
- JSON 导入：支持 `entries` 和 `records` 根字段。
- 文本导入：支持 `=>`、`->`、Tab 和逗号分隔。
- 导出：集中生成 CSV 和可读文本。
- 搜索：集中判断原词、标准写法、别名、备注、作用范围和匹配方式。

## 对 11-20 架构任务的影响

- 12：词库页还不是独立 QWidget，但业务规则已经从 methods 头文件移出一层。
- 17：词库页 UI 和词库导入导出规则开始分离。
- 18：词库作用范围和导入导出数据结构开始显式化，后续迁移词库页时不需要继续复制这些规则。

## 验证

- `vocabulary_io_tests` 覆盖自定义作用范围、中文 CSV 表头、JSON records 导入、可读文本导出和搜索筛选。

## 后续

1. 把词库页 UI 本体迁成 `VocabularyPage`。
2. 把词库新增/编辑弹窗迁成独立对话类。
3. 把候选推荐弹窗迁成独立对话类，并复用 `vocabulary_io` 的作用范围规则。
