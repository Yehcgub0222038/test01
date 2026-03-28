#include <tcl.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <random>

/* ================================================================= *
 * 輔助工具函數
 * ================================================================= */

 // 多層次比較邏輯 (用於穩定排序)
bool CompareRows(const std::vector<double>& a, const std::vector<double>& b) {
    size_t min_cols = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_cols; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return a.size() < b.size();
}

/* ================================================================= *
 * 指令實作
 * ================================================================= */

 // --- 1. math::multiSort arg1 ?arg2 ...? ---
 // 支援 1D List 或 2D Matrix 混合輸入，自動橫向展開後進行多層次排序
int MultiSortObjCmd(ClientData cd, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "list_or_matrix ?...?");
        return TCL_ERROR;
    }

    int rowCount = -1;
    std::vector<std::vector<double>> finalMatrix;

    // 第一階段：檢查輸入合法性並確定總行數
    for (int i = 1; i < objc; i++) {
        int listLen;
        Tcl_Obj** listPtrs;
        if (Tcl_ListObjGetElements(interp, objv[i], &listLen, &listPtrs) != TCL_OK) return TCL_ERROR;
        if (listLen == 0) continue;

        if (rowCount == -1) rowCount = listLen;
        else if (rowCount != listLen) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("All inputs must have the same number of rows", -1));
            return TCL_ERROR;
        }
    }

    if (rowCount <= 0) return TCL_OK;
    finalMatrix.resize(rowCount);

    // 第二階段：填充矩陣數據
    for (int i = 1; i < objc; i++) {
        int listLen; Tcl_Obj** listPtrs;
        Tcl_ListObjGetElements(interp, objv[i], &listLen, &listPtrs);
        if (listLen == 0) continue;

        int subLen; Tcl_Obj** subPtrs;
        // 判斷是否為 2D List (Matrix)
        bool is2D = (Tcl_ListObjGetElements(NULL, listPtrs[0], &subLen, &subPtrs) == TCL_OK);

        for (int r = 0; r < rowCount; r++) {
            if (is2D) {
                int cols; Tcl_Obj** colData;
                Tcl_ListObjGetElements(interp, listPtrs[r], &cols, &colData);
                for (int c = 0; c < cols; c++) {
                    double v; Tcl_GetDoubleFromObj(interp, colData[c], &v);
                    finalMatrix[r].push_back(v);
                }
            }
            else {
                double v; Tcl_GetDoubleFromObj(interp, listPtrs[r], &v);
                finalMatrix[r].push_back(v);
            }
        }
    }

    std::stable_sort(finalMatrix.begin(), finalMatrix.end(), CompareRows);

    // 第三階段：打包回傳
    Tcl_Obj* resList = Tcl_NewListObj(0, NULL);
    for (size_t r = 0; r < finalMatrix.size(); r++) {
        Tcl_Obj* rowObj = Tcl_NewListObj(0, NULL);
        for (double val : finalMatrix[r]) Tcl_ListObjAppendElement(interp, rowObj, Tcl_NewDoubleObj(val));
        Tcl_ListObjAppendElement(interp, resList, rowObj);
    }
    Tcl_SetObjResult(interp, resList);
    return TCL_OK;
}

// --- 2. math::mergeList matrix N ---
// 根據前 N 欄合併重複行，並將剩餘資料橫向展開
int MergeListObjCmd(ClientData cd, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "matrix N");
        return TCL_ERROR;
    }

    int N, rowCount;
    Tcl_Obj** rowPtrs;
    if (Tcl_ListObjGetElements(interp, objv[1], &rowCount, &rowPtrs) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[2], &N) != TCL_OK) return TCL_ERROR;

    if (rowCount == 0 || N <= 0) {
        Tcl_SetObjResult(interp, objv[1]);
        return TCL_OK;
    }

    std::map<std::vector<double>, std::vector<double>> aggMap;
    for (int i = 0; i < rowCount; i++) {
        int colCount; Tcl_Obj** colPtrs;
        if (Tcl_ListObjGetElements(interp, rowPtrs[i], &colCount, &colPtrs) != TCL_OK) return TCL_ERROR;

        std::vector<double> key, rest;
        for (int j = 0; j < colCount; j++) {
            double v; Tcl_GetDoubleFromObj(interp, colPtrs[j], &v);
            if (j < N) key.push_back(v); else rest.push_back(v);
        }

        if (aggMap.find(key) == aggMap.end()) aggMap[key] = rest;
        else aggMap[key].insert(aggMap[key].end(), rest.begin(), rest.end());
    }

    Tcl_Obj* resList = Tcl_NewListObj(0, NULL);
    for (auto it = aggMap.begin(); it != aggMap.end(); ++it) {
        Tcl_Obj* newRow = Tcl_NewListObj(0, NULL);
        for (double v : it->first) Tcl_ListObjAppendElement(interp, newRow, Tcl_NewDoubleObj(v));
        for (double v : it->second) Tcl_ListObjAppendElement(interp, newRow, Tcl_NewDoubleObj(v));
        Tcl_ListObjAppendElement(interp, resList, newRow);
    }
    Tcl_SetObjResult(interp, resList);
    return TCL_OK;
}

// --- 3. math::randomList size ---
// 產生 0 ~ size-1 的隨機洗牌整數清單
int RandomListObjCmd(ClientData cd, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "size");
        return TCL_ERROR;
    }
    int n;
    if (Tcl_GetIntFromObj(interp, objv[1], &n) != TCL_OK) return TCL_ERROR;
    if (n <= 0) return TCL_OK;

    std::vector<int> vec(n);
    std::iota(vec.begin(), vec.end(), 0);
    std::random_device rd; std::mt19937 g(rd());
    std::shuffle(vec.begin(), vec.end(), g);

    Tcl_Obj* resList = Tcl_NewListObj(0, NULL);
    for (int v : vec) Tcl_ListObjAppendElement(interp, resList, Tcl_NewIntObj(v));
    Tcl_SetObjResult(interp, resList);
    return TCL_OK;
}

/* ================================================================= *
 * DLL 初始化入口
 * ================================================================= */

extern "C" __declspec(dllexport) int combinedext_Init(Tcl_Interp* interp) {
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) return TCL_ERROR;

    Tcl_CreateObjCommand(interp, "math::multiSort", MultiSortObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "math::mergeList", MergeListObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "math::randomList", RandomListObjCmd, NULL, NULL);

    Tcl_PkgProvide(interp, "combinedext", "1.0");
    return TCL_OK;
}
