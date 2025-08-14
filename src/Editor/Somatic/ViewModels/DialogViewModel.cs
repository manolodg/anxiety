using CommunityToolkit.Mvvm.ComponentModel;

namespace Somatic.ViewModels {
    public partial class DialogViewModel : ViewModelBase {
        [ObservableProperty] private PageViewModel _currentPage = null!;
    }    
}
