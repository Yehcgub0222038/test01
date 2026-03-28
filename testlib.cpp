#include <tcl.h>
#include <vector>
#include <algorithm>
#include <numeric>   // std::iota
#include <random>    // std::random_device, std::mt19937

// --- 工具函數：多層次比較邏輯 ---
bool CompareRows(const std::vector<double>& a, const std::vector<double>& b) {
    size_t cols = a.size();
    for (size_t i = 0; i < cols; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false;
}

// --- 指令 1: math::multiSort list1 list2 ... ---
int MultiSortObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "list1 ?list2 ...?");
        return TCL_ERROR;
    }
    int numLists = objc - 1;
    int rowCount = 0;
    std::vector<int> listLens(numLists);
    std::vector<Tcl_Obj**> listPtrs(numLists);

    for (int i = 0; i < numLists; ++i) {
        if (Tcl_ListObjGetElements(interp, objv[i + 1], &listLens[i], &listPtrs[i]) != TCL_OK) return TCL_ERROR;
        if (i == 0) rowCount = listLens[i];
        else if (listLens[i] != rowCount) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("All lists must have the same length", -1));
            return TCL_ERROR;
        }
    }
    if (rowCount == 0) return TCL_OK;

    std::vector<std::vector<double>> matrix(rowCount, std::vector<double>(numLists));
    for (int r = 0; r < rowCount; ++r) {
        for (int c = 0; c < numLists; ++c) {
            double val;
            if (Tcl_GetDoubleFromObj(interp, listPtrs[c][r], &val) != TCL_OK) return TCL_ERROR;
            matrix[r][c] = val;
        }
    }

    std::stable_sort(matrix.begin(), matrix.end(), CompareRows);

    Tcl_Obj *resList = Tcl_NewListObj(0, NULL);
    for (int r = 0; r < rowCount; ++r) {
        Tcl_Obj *rowObj = Tcl_NewListObj(0, NULL);
        for (int c = 0; c < numLists; ++c) Tcl_ListObjAppendElement(interp, rowObj, Tcl_NewDoubleObj(matrix[r][c]));
        Tcl_ListObjAppendElement(interp, resList, rowObj);
    }
    Tcl_SetObjResult(interp, resList);
    return TCL_OK;
}

// --- 指令 2: math::randomList size ---
int RandomListObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "size");
        return TCL_ERROR;
    }

    int n;
    if (Tcl_GetIntFromObj(interp, objv[1], &n) != TCL_OK) return TCL_ERROR;
    if (n <= 0) {
        Tcl_SetObjResult(interp, Tcl_NewListObj(0, NULL));
        return TCL_OK;
    }

    // 1. 產生 0 ~ n-1 的 vector
    std::vector<int> vec(n);
    std::iota(vec.begin(), vec.end(), 0);

    // 2. 隨機洗牌
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(vec.begin(), vec.end(), g);

    // 3. 轉為 Tcl List
    Tcl_Obj *resList = Tcl_NewListObj(0, NULL);
    for (int val : vec) {
        Tcl_ListObjAppendElement(interp, resList, Tcl_NewIntObj(val));
    }

    Tcl_SetObjResult(interp, resList);
    return TCL_OK;
}

// --- 初始化入口 ---
extern "C" __declspec(dllexport) int Multisort_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.5", 0) == NULL) return TCL_ERROR;

    // 註冊兩個指令
    Tcl_CreateObjCommand(interp, "math::multiSort", MultiSortObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "math::randomList", RandomListObjCmd, NULL, NULL);

    Tcl_PkgProvide(interp, "MultiSort", "1.0");
    return TCL_OK;
}
