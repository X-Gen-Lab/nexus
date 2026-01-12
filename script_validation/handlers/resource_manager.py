"""
资源管理器

管理临时文件和资源清理，处理脚本中断时的状态恢复。

需求：3.4, 4.4
"""

import os
import shutil
import tempfile
import atexit
import signal
import threading
import time
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any, Callable, Set
from datetime import datetime
from contextlib import contextmanager
import json


@dataclass
class ResourceState:
    """资源状态"""
    resource_id: str
    resource_type: str  # 'file', 'directory', 'process', 'lock'
    path: Optional[Path] = None
    created_at: datetime = field(default_factory=datetime.now)
    metadata: Dict[str, Any] = field(default_factory=dict)
    cleanup_callback: Optional[Callable] = None


@dataclass
class ExecutionState:
    """执行状态快照"""
    state_id: str
    script_path: Optional[Path] = None
    timestamp: datetime = field(default_factory=datetime.now)
    working_directory: Optional[Path] = None
    environment_variables: Dict[str, str] = field(default_factory=dict)
    created_files: List[Path] = field(default_factory=list)
    modified_files: Dict[str, bytes] = field(default_factory=dict)  # path -> original content
    metadata: Dict[str, Any] = field(default_factory=dict)


class ResourceManager:
    """
    资源管理器

    管理临时文件和资源清理，处理脚本中断时的状态恢复。

    需求：3.4, 4.4
    """

    def __init__(self, base_temp_dir: Optional[Path] = None):
        """初始化资源管理器"""
        self._base_temp_dir = base_temp_dir or Path(tempfile.gettempdir()) / 'script_validation'
        self._resources: Dict[str, ResourceState] = {}
        self._execution_states: Dict[str, ExecutionState] = {}
        self._lock = threading.Lock()
        self._cleanup_registered = False
        self._interrupted = False

        # 确保基础临时目录存在
        self._base_temp_dir.mkdir(parents=True, exist_ok=True)

        # 注册清理回调
        self._register_cleanup_handlers()

    # ========================================================================
    # 临时文件管理
    # ========================================================================

    def create_temp_file(self, prefix: str = 'script_',
                        suffix: str = '.tmp',
                        content: Optional[str] = None) -> Path:
        """
        创建临时文件

        需求 4.4: 验证正确清理和磁盘空间管理
        """
        with self._lock:
            # 创建临时文件
            fd, temp_path = tempfile.mkstemp(
                prefix=prefix,
                suffix=suffix,
                dir=str(self._base_temp_dir)
            )

            try:
                if content:
                    os.write(fd, content.encode('utf-8'))
            finally:
                os.close(fd)

            temp_path = Path(temp_path)

            # 注册资源
            resource_id = f"file_{temp_path.name}"
            self._resources[resource_id] = ResourceState(
                resource_id=resource_id,
                resource_type='file',
                path=temp_path
            )

            return temp_path

    def create_temp_directory(self, prefix: str = 'script_') -> Path:
        """
        创建临时目录

        需求 4.4: 验证正确清理和磁盘空间管理
        """
        with self._lock:
            # 创建临时目录
            temp_dir = Path(tempfile.mkdtemp(
                prefix=prefix,
                dir=str(self._base_temp_dir)
            ))

            # 注册资源
            resource_id = f"dir_{temp_dir.name}"
            self._resources[resource_id] = ResourceState(
                resource_id=resource_id,
                resource_type='directory',
                path=temp_dir
            )

            return temp_dir

    def register_resource(self, resource_id: str, resource_type: str,
                         path: Optional[Path] = None,
                         cleanup_callback: Optional[Callable] = None,
                         metadata: Optional[Dict[str, Any]] = None) -> str:
        """注册需要管理的资源"""
        with self._lock:
            self._resources[resource_id] = ResourceState(
                resource_id=resource_id,
                resource_type=resource_type,
                path=path,
                cleanup_callback=cleanup_callback,
                metadata=metadata or {}
            )
            return resource_id

    def unregister_resource(self, resource_id: str) -> bool:
        """取消注册资源（不执行清理）"""
        with self._lock:
            if resource_id in self._resources:
                del self._resources[resource_id]
                return True
            return False

    def cleanup_resource(self, resource_id: str) -> bool:
        """
        清理单个资源

        需求 3.4: 清理临时文件
        """
        with self._lock:
            if resource_id not in self._resources:
                return False

            resource = self._resources[resource_id]
            success = self._cleanup_single_resource(resource)

            if success:
                del self._resources[resource_id]

            return success

    def cleanup_all_resources(self) -> Dict[str, bool]:
        """
        清理所有注册的资源

        需求 3.4: 清理临时文件
        """
        results = {}

        with self._lock:
            resource_ids = list(self._resources.keys())

            for resource_id in resource_ids:
                resource = self._resources[resource_id]
                success = self._cleanup_single_resource(resource)
                results[resource_id] = success

                if success:
                    del self._resources[resource_id]

        return results

    def get_registered_resources(self) -> List[ResourceState]:
        """获取所有注册的资源"""
        with self._lock:
            return list(self._resources.values())


    # ========================================================================
    # 状态保存和恢复
    # ========================================================================

    def save_execution_state(self, script_path: Optional[Path] = None,
                            files_to_backup: Optional[List[Path]] = None,
                            metadata: Optional[Dict[str, Any]] = None) -> str:
        """
        保存执行状态快照

        需求 3.4: 在可能的情况下恢复原始状态
        """
        with self._lock:
            state_id = f"state_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}"

            # 备份指定文件的内容
            modified_files = {}
            if files_to_backup:
                for file_path in files_to_backup:
                    if file_path.exists() and file_path.is_file():
                        try:
                            modified_files[str(file_path)] = file_path.read_bytes()
                        except (IOError, PermissionError):
                            pass

            # 创建状态快照
            state = ExecutionState(
                state_id=state_id,
                script_path=script_path,
                working_directory=Path.cwd(),
                environment_variables=dict(os.environ),
                modified_files=modified_files,
                metadata=metadata or {}
            )

            self._execution_states[state_id] = state

            # 持久化状态到文件
            self._persist_state(state)

            return state_id

    def restore_execution_state(self, state_id: str) -> bool:
        """
        恢复执行状态

        需求 3.4: 在可能的情况下恢复原始状态
        """
        with self._lock:
            state = self._execution_states.get(state_id)

            if not state:
                # 尝试从持久化存储加载
                state = self._load_persisted_state(state_id)
                if not state:
                    return False

            success = True

            # 恢复备份的文件
            for file_path_str, content in state.modified_files.items():
                try:
                    file_path = Path(file_path_str)
                    file_path.parent.mkdir(parents=True, exist_ok=True)
                    file_path.write_bytes(content)
                except (IOError, PermissionError) as e:
                    success = False

            # 删除创建的文件
            for created_file in state.created_files:
                try:
                    if created_file.exists():
                        if created_file.is_file():
                            created_file.unlink()
                        elif created_file.is_dir():
                            shutil.rmtree(created_file)
                except (IOError, PermissionError):
                    success = False

            return success

    def discard_execution_state(self, state_id: str) -> bool:
        """丢弃执行状态（不恢复）"""
        with self._lock:
            if state_id in self._execution_states:
                del self._execution_states[state_id]

                # 删除持久化文件
                state_file = self._base_temp_dir / f"{state_id}.json"
                if state_file.exists():
                    state_file.unlink()

                return True
            return False

    def track_created_file(self, state_id: str, file_path: Path) -> bool:
        """跟踪在执行期间创建的文件"""
        with self._lock:
            if state_id in self._execution_states:
                self._execution_states[state_id].created_files.append(file_path)
                return True
            return False

    def get_execution_state(self, state_id: str) -> Optional[ExecutionState]:
        """获取执行状态"""
        with self._lock:
            return self._execution_states.get(state_id)

    # ========================================================================
    # 上下文管理器
    # ========================================================================

    @contextmanager
    def managed_temp_file(self, prefix: str = 'script_',
                         suffix: str = '.tmp',
                         content: Optional[str] = None):
        """
        上下文管理器：自动清理的临时文件

        需求 4.4: 验证正确清理和磁盘空间管理
        """
        temp_path = self.create_temp_file(prefix, suffix, content)
        try:
            yield temp_path
        finally:
            self.cleanup_resource(f"file_{temp_path.name}")

    @contextmanager
    def managed_temp_directory(self, prefix: str = 'script_'):
        """
        上下文管理器：自动清理的临时目录

        需求 4.4: 验证正确清理和磁盘空间管理
        """
        temp_dir = self.create_temp_directory(prefix)
        try:
            yield temp_dir
        finally:
            self.cleanup_resource(f"dir_{temp_dir.name}")

    @contextmanager
    def execution_context(self, script_path: Optional[Path] = None,
                         files_to_backup: Optional[List[Path]] = None,
                         metadata: Optional[Dict[str, Any]] = None):
        """
        上下文管理器：执行上下文，支持中断恢复

        需求 3.4: 清理临时文件并在可能的情况下恢复原始状态
        """
        state_id = self.save_execution_state(script_path, files_to_backup, metadata)

        try:
            yield state_id
            # 正常完成，丢弃状态
            self.discard_execution_state(state_id)
        except (KeyboardInterrupt, SystemExit):
            # 中断，恢复状态
            self._interrupted = True
            self.restore_execution_state(state_id)
            raise
        except Exception:
            # 其他异常，恢复状态
            self.restore_execution_state(state_id)
            raise

    # ========================================================================
    # 中断处理
    # ========================================================================

    def handle_interrupt(self, signum: int = None, frame: Any = None):
        """
        处理中断信号

        需求 3.4: 脚本在执行期间被中断时，清理临时文件并恢复原始状态
        """
        self._interrupted = True

        # 恢复所有执行状态
        for state_id in list(self._execution_states.keys()):
            try:
                self.restore_execution_state(state_id)
            except Exception:
                pass

        # 清理所有资源
        self.cleanup_all_resources()

    def is_interrupted(self) -> bool:
        """检查是否被中断"""
        return self._interrupted

    def reset_interrupt_flag(self):
        """重置中断标志"""
        self._interrupted = False


    # ========================================================================
    # 磁盘空间管理
    # ========================================================================

    def get_temp_directory_size(self) -> int:
        """
        获取临时目录大小（字节）

        需求 4.4: 磁盘空间管理
        """
        total_size = 0

        if self._base_temp_dir.exists():
            for item in self._base_temp_dir.rglob('*'):
                if item.is_file():
                    try:
                        total_size += item.stat().st_size
                    except (OSError, PermissionError):
                        pass

        return total_size

    def get_disk_free_space(self, path: Optional[Path] = None) -> int:
        """
        获取磁盘可用空间（字节）

        需求 4.4: 磁盘空间管理
        """
        check_path = path or self._base_temp_dir

        try:
            if os.name == 'nt':  # Windows
                import ctypes
                free_bytes = ctypes.c_ulonglong(0)
                ctypes.windll.kernel32.GetDiskFreeSpaceExW(
                    ctypes.c_wchar_p(str(check_path)),
                    None, None,
                    ctypes.pointer(free_bytes)
                )
                return free_bytes.value
            else:  # Unix/Linux
                stat = os.statvfs(check_path)
                return stat.f_bavail * stat.f_frsize
        except Exception:
            return -1

    def cleanup_old_temp_files(self, max_age_hours: int = 24) -> int:
        """
        清理旧的临时文件

        需求 4.4: 磁盘空间管理
        """
        cleaned_count = 0
        cutoff_time = time.time() - (max_age_hours * 3600)

        if self._base_temp_dir.exists():
            for item in self._base_temp_dir.iterdir():
                try:
                    if item.stat().st_mtime < cutoff_time:
                        if item.is_file():
                            item.unlink()
                            cleaned_count += 1
                        elif item.is_dir():
                            shutil.rmtree(item)
                            cleaned_count += 1
                except (OSError, PermissionError):
                    pass

        return cleaned_count

    def ensure_disk_space(self, required_bytes: int) -> bool:
        """
        确保有足够的磁盘空间

        需求 4.4: 磁盘空间管理
        """
        free_space = self.get_disk_free_space()

        if free_space < 0:
            return True  # 无法检测，假设有足够空间

        if free_space >= required_bytes:
            return True

        # 尝试清理旧文件
        self.cleanup_old_temp_files(max_age_hours=1)

        # 再次检查
        free_space = self.get_disk_free_space()
        return free_space >= required_bytes

    # ========================================================================
    # 私有方法
    # ========================================================================

    def _cleanup_single_resource(self, resource: ResourceState) -> bool:
        """清理单个资源"""
        success = True

        # 执行自定义清理回调
        if resource.cleanup_callback:
            try:
                resource.cleanup_callback()
            except Exception:
                success = False

        # 清理文件或目录
        if resource.path and resource.path.exists():
            try:
                if resource.resource_type == 'file':
                    resource.path.unlink()
                elif resource.resource_type == 'directory':
                    shutil.rmtree(resource.path)
            except (OSError, PermissionError):
                success = False

        return success

    def _register_cleanup_handlers(self):
        """注册清理处理器"""
        if self._cleanup_registered:
            return

        # 注册退出时清理
        atexit.register(self._cleanup_on_exit)

        # 注册信号处理器（仅在支持的平台上）
        try:
            signal.signal(signal.SIGINT, self._signal_handler)
            signal.signal(signal.SIGTERM, self._signal_handler)
        except (ValueError, OSError):
            # 在某些环境中可能无法设置信号处理器
            pass

        self._cleanup_registered = True

    def _cleanup_on_exit(self):
        """退出时清理"""
        self.cleanup_all_resources()

    def _signal_handler(self, signum: int, frame: Any):
        """信号处理器"""
        self.handle_interrupt(signum, frame)
        # 重新引发信号以允许默认处理
        signal.signal(signum, signal.SIG_DFL)
        os.kill(os.getpid(), signum)

    def _persist_state(self, state: ExecutionState):
        """持久化状态到文件"""
        state_file = self._base_temp_dir / f"{state.state_id}.json"

        try:
            state_data = {
                'state_id': state.state_id,
                'script_path': str(state.script_path) if state.script_path else None,
                'timestamp': state.timestamp.isoformat(),
                'working_directory': str(state.working_directory) if state.working_directory else None,
                'environment_variables': state.environment_variables,
                'created_files': [str(f) for f in state.created_files],
                'modified_files': {k: v.hex() for k, v in state.modified_files.items()},
                'metadata': state.metadata
            }

            with open(state_file, 'w', encoding='utf-8') as f:
                json.dump(state_data, f, indent=2)
        except (IOError, PermissionError):
            pass

    def _load_persisted_state(self, state_id: str) -> Optional[ExecutionState]:
        """从文件加载持久化状态"""
        state_file = self._base_temp_dir / f"{state_id}.json"

        if not state_file.exists():
            return None

        try:
            with open(state_file, 'r', encoding='utf-8') as f:
                state_data = json.load(f)

            return ExecutionState(
                state_id=state_data['state_id'],
                script_path=Path(state_data['script_path']) if state_data['script_path'] else None,
                timestamp=datetime.fromisoformat(state_data['timestamp']),
                working_directory=Path(state_data['working_directory']) if state_data['working_directory'] else None,
                environment_variables=state_data['environment_variables'],
                created_files=[Path(f) for f in state_data['created_files']],
                modified_files={k: bytes.fromhex(v) for k, v in state_data['modified_files'].items()},
                metadata=state_data['metadata']
            )
        except (IOError, json.JSONDecodeError, KeyError):
            return None

    def __del__(self):
        """析构函数：确保资源被清理"""
        try:
            self.cleanup_all_resources()
        except Exception:
            pass
